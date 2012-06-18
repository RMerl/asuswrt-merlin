/*
 * $Id: configfile.c,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Functions for reading and writing the config file
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
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


/**
 * \file configfile.c
 * 
 * Configfile and web interface handling.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _POSIX_PTHREAD_SEMANTICS
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <restart.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include "configfile.h"
#include "err.h"

#ifndef WITHOUT_MDNS
# include "rend.h"
#endif


/*
 * Forwards
 */
static void config_emit_string(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_literal(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_int(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_include(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_threadstatus(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_ispage(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_session_count(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_service_status(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_user(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_readonly(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_version(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_system(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_flags(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_subst_stream(WS_CONNINFO *pwsc, int fd_src);
static int config_file_is_readonly(void);
static int config_mutex_lock(void);
static int config_mutex_unlock(void);
static int config_existdir(char *path);
static int config_makedir(char *path);

/*
 * Defines
 */
#define CONFIG_TYPE_INT       0  /**< Config element is an integer */
#define CONFIG_TYPE_STRING    1  /**< Config element is a string */
#define CONFIG_TYPE_SPECIAL   4  /**< Config element is a web  parameter */

/** Every config file directive and web interface directive is a CONFIGELEMENT */
typedef struct tag_configelement {
    int config_element;    /**< config file directive or web interface directive */
    int required;          /**< If config file, is it required? */
    int changed;           /**< Has this been changed since the last load? */
    int type;              /**< Int, string, or special? */
    char *name;            /**< config file directive name */
    void *var;             /**< if config file, where is the corresponding var? */
    void (*emit)(WS_CONNINFO *, void *, char *);  /* how to display it on a web page */
} CONFIGELEMENT;

/** List of all valid config entries and web interface directives */
CONFIGELEMENT config_elements[] = {
    { 1,1,0,CONFIG_TYPE_STRING,"runas",(void*)&config.runas,config_emit_string },
    { 1,1,0,CONFIG_TYPE_STRING,"web_root",(void*)&config.web_root,config_emit_string },
    { 1,1,0,CONFIG_TYPE_INT,"port",(void*)&config.port,config_emit_int },
    { 1,1,0,CONFIG_TYPE_STRING,"admin_pw",(void*)&config.adminpassword,config_emit_string },
    { 1,1,0,CONFIG_TYPE_STRING,"mp3_dir",(void*)&config.mp3dir,config_emit_string },
#ifdef WITH_GDBM
    { 1,1,0,CONFIG_TYPE_STRING,"db_dir",(void*)&config.dbdir,config_emit_string },
#else
    { 1,0,0,CONFIG_TYPE_STRING,"db_dir",(void*)&config.dbdir,config_emit_string },
#endif
    { 1,1,0,CONFIG_TYPE_STRING,"servername",(void*)&config.servername,config_emit_string },
    { 1,0,0,CONFIG_TYPE_INT,"rescan_interval",(void*)&config.rescan_interval,config_emit_int },
    { 1,0,0,CONFIG_TYPE_INT,"always_scan",(void*)&config.always_scan,config_emit_int },
    { 1,0,0,CONFIG_TYPE_INT,"process_m3u",(void*)&config.process_m3u,config_emit_int },
    { 1,0,0,CONFIG_TYPE_INT,"scan_type",(void*)&config.scan_type,config_emit_int },
    { 1,0,0,CONFIG_TYPE_INT,"compress",(void*)&config.compress,config_emit_int },
    { 1,0,0,CONFIG_TYPE_STRING,"playlist",(void*)&config.playlist,config_emit_string },
    { 1,0,0,CONFIG_TYPE_STRING,"extensions",(void*)&config.extensions,config_emit_string },
    { 1,0,0,CONFIG_TYPE_STRING,"interface",(void*)&config.iface,config_emit_string },
    { 1,0,0,CONFIG_TYPE_STRING,"password",(void*)&config.readpassword, config_emit_string },
    { 1,0,0,CONFIG_TYPE_STRING,"logfile",(void*)&config.logfile, config_emit_string },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"release",(void*)VERSION,config_emit_literal },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"package",(void*)PACKAGE,config_emit_literal },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"include",(void*)NULL,config_emit_include },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"threadstat",(void*)NULL,config_emit_threadstatus },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"ispage",(void*)NULL,config_emit_ispage },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"session-count",(void*)NULL,config_emit_session_count },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"service-status",(void*)NULL,config_emit_service_status },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"user",(void*)NULL,config_emit_user },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"readonly",(void*)NULL,config_emit_readonly },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"version",(void*)NULL,config_emit_version },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"system",(void*)NULL,config_emit_system },
    { 1,0,0,CONFIG_TYPE_STRING,"art_filename",(void*)&config.artfilename,config_emit_string },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"flags",(void*)NULL,config_emit_flags },
    { -1,1,0,CONFIG_TYPE_STRING,NULL,NULL,NULL }
};

/** table of thread status -- as displayed on the status page */
typedef struct tag_scan_status {
    int session;
    int thread;
    char *what;
    char *host;
    struct tag_scan_status *next;
} SCAN_STATUS;

SCAN_STATUS scan_status = { 0,0,NULL,NULL };            /**< root of status list */
pthread_mutex_t scan_mutex = PTHREAD_MUTEX_INITIALIZER; /**< status list mutex */
int config_session=0;                                   /**< session counter */

#define MAX_LINE 1024


/**
 * Try and create a directory, including parents.
 * 
 * \param path path to make
 * \returns 0 on success, -1 otherwise, with errno set
 */

int config_makedir(char *path) {
    char *token, *next_token;
    char *pathdup;
    char path_buffer[PATH_MAX];
    int err;

    DPRINTF(E_DBG,L_CONF,"Creating %s\n",path);

    pathdup=strdup(path);
    if(!pathdup) {
	return -1;
    }

    next_token=pathdup+1;
    memset(path_buffer,0,sizeof(path_buffer));

    while((token=strsep(&next_token,"/"))) {
	if((strlen(path_buffer) + strlen(token)) < PATH_MAX) {
	    strcat(path_buffer,"/");
	    strcat(path_buffer,token);
	    DPRINTF(E_DBG,L_CONF,"Making %s\n",path_buffer);
	    if((mkdir(path_buffer,0700)) && (errno != EEXIST)) {
		err=errno;
		free(pathdup);
		errno=err;
		return -1;  
	    }
	} else {
	    errno=ENAMETOOLONG;
	    return -1; 
	}
    }

    free(pathdup);
    return 0;
}

/**
 * Determine if a particular directory exists or not
 *
 * \param path directory to test for existence
 */
int config_existdir(char *path) {
    struct stat sb;

    DPRINTF(E_DBG,L_CONF,"Checking existence of %s\n",path);

    if(stat(path,&sb)) {
	return 0;
    }

    if(sb.st_mode & S_IFDIR)
	return 1;

    errno=ENOTDIR;
    return 0;
}


/**
 * Read the specified config file, padding the config structure
 * appropriately.
 *
 * \param file config file to process
 * \returns 0 on success, -1 otherwise with errno set
 */
int config_read(char *file) {
    FILE *fin;
    char *buffer;
    int err=0;
    char *value;
    char *comment;
    char path_buffer[PATH_MAX];
    CONFIGELEMENT *pce;
    int handled;
    char *term;

    buffer=(char*)malloc(MAX_LINE+1);
    if(!buffer)
	return -1;

    if((fin=fopen(file,"r")) == NULL) {
	err=errno;
	free(buffer);

	if(ENOENT == err) {
	    DPRINTF(E_LOG,L_CONF,"Whoops!  Can't find the config file!  If you are running this for the first\n");
	    DPRINTF(E_LOG,L_CONF,"time, then perhaps you've forgotten to copy the sample config in the \n");
	    DPRINTF(E_LOG,L_CONF,"contrib directory into /etc/mt-daapd.conf.  Just a suggestion...\n\n");
	}

	errno=err;
	return -1;
    }

#ifdef NSLU2    
    config.always_scan=0;
#else
    config.always_scan=1;
#endif

    config.configfile=strdup(file);
    config.web_root=NULL;
    config.adminpassword=NULL;
    config.readpassword=NULL;
    config.iface=NULL;
    config.mp3dir=NULL;
    config.playlist=NULL;
    config.runas=NULL;
    config.artfilename=NULL;
    config.logfile=NULL;
    config.rescan_interval=0;
    config.process_m3u=0;
    config.scan_type=0;
    config.compress=0;

    /* DWB: use alloced space so it can be freed without errors */
    config.extensions=strdup(".mp3");

    /* DWB: use alloced space so it can be freed without errors */
    config.servername=strdup("mt-daapd " VERSION);

    while(fgets(buffer,MAX_LINE,fin)) {
	buffer[MAX_LINE] = '\0';

	comment=strchr(buffer,'#');
	if(comment)
	    *comment = '\0';

	while(strlen(buffer) && (strchr("\n\r ",buffer[strlen(buffer)-1])))
	    buffer[strlen(buffer)-1] = '\0';

	term=buffer;

	while((*term=='\t') || (*term==' '))
	    term++;

	value=term;

	strsep(&value,"\t ");
	if((value) && (term) && (strlen(term))) {
	    while(strlen(value) && (strchr("\t ",*value)))
		value++;
	    
	    pce=config_elements;
	    handled=0;
	    while((!handled) && (pce->config_element != -1)) {
		if((strcasecmp(term,pce->name)==0) && (pce->config_element)) {
		    /* valid config directive */
		    handled=1;
		    pce->changed=1;
		    
		    DPRINTF(E_DBG,L_CONF,"Read %s: %s\n",pce->name,value);
		    
		    switch(pce->type) {
		    case CONFIG_TYPE_STRING:
			/* DWB: free space to prevent small leak */
			if(*((char **)(pce->var)))
			    free(*((char **)(pce->var)));
			*((char **)(pce->var)) = (void*)strdup(value);
			break;
		    case CONFIG_TYPE_INT:
			*((int*)(pce->var)) = atoi(value);
			break;
		    }
		}
		pce++;
	    }
	    
	    if(!handled) {
		fprintf(stderr,"Invalid config directive: %s\n",buffer);
		fclose(fin);
		return -1;
	    }
	}
    }

    fclose(fin);
    free(buffer);

    /* Set the directory components to realpaths */
    realpath(config.web_root,path_buffer);
    free(config.web_root);
    config.web_root=strdup(path_buffer);

    realpath(config.mp3dir,path_buffer);
    free(config.mp3dir);
    config.mp3dir=strdup(path_buffer);

    realpath(config.dbdir,path_buffer);
    free(config.dbdir);
    config.dbdir=strdup(path_buffer);

    /* check to see if all required elements are satisfied */
    pce=config_elements;
    err=0;
    while((pce->config_element != -1)) {
	if(pce->required && pce->config_element && !pce->changed) {
	    DPRINTF(E_LOG,L_CONF,"Required config entry '%s' not specified\n",pce->name);
	    err=-1;
	}

	/* too much spam on startup
	if((pce->config_element) && (pce->changed)) {
	    switch(pce->type) {
	    case CONFIG_TYPE_STRING:
		DPRINTF(E_INF,"%s: %s\n",pce->name,*((char**)pce->var));
		break;
	    case CONFIG_TYPE_INT:
		DPRINTF(E_INF,"%s: %d\n",pce->name,*((int*)pce->var));
		break;
	    }
	}
	*/

	pce->changed=0;
	pce++;
    }
    

    /* sanity check the paths */
    sprintf(path_buffer,"%s/index.html",config.web_root);
    if((fin=fopen(path_buffer,"r")) == NULL) {
	err=-1;
	DPRINTF(E_LOG,L_CONF,"Invalid web_root\n");

	/* check for the common error */
	if(strcasecmp(config.web_root,"/usr/share/mt-daapd/admin-root") == 0) {
	    /* see if /usr/local is any better */
	    if((fin=fopen("/usr/local/share/mt-daapd/admin-root","r")) != NULL) {
		fclose(fin);
		DPRINTF(E_LOG,L_CONF,"Should it be /usr/local/share/mt-daapd/admin-root?");
	    }
	}
    } else {
	fclose(fin);
    }

    /* should really check the mp3 path */
    if(!config_existdir(config.mp3dir)) {
	DPRINTF(E_LOG,L_CONF,"Bad mp3 directory (%s): %s\n",config.mp3dir,strerror(errno));
	return -1;
    }

    if(!config_existdir(config.dbdir)) {
	/* try to make it */
	if(config_makedir(config.dbdir)) {
	    DPRINTF(E_LOG,L_CONF,"Database dir %s does not exist, cannot create: %s\n",
		    config.dbdir,strerror(errno));
	    return -1;
	}
    }

    /* must have zlib 1.2 or better for gzip encoding support */
    if(!strncmp(ZLIB_VERSION,"0.",2) ||
       !strncmp(ZLIB_VERSION,"1.0",3) ||
       !strncmp(ZLIB_VERSION,"1.1",3)) {
	if(config.compress) {
	    config.compress=0;
	    DPRINTF(E_LOG,L_CONF,"Must have zlib > 1.2.0 to use gzip content encoding.  You have %s.  Disabling.\n",ZLIB_VERSION);
	}
    }

    return err;
}


/**
 * free up any memory used
 */
void config_close(void) {
    CONFIGELEMENT *pce;
    int err;

    /* check to see if all required elements are satisfied */
    free(config.configfile);
    pce=config_elements;
    err=0;
    while((pce->config_element != -1)) {
	if((pce->config_element) && 
	   (pce->type == CONFIG_TYPE_STRING) && 
	   (*((char**)pce->var))) {
	    DPRINTF(E_DBG,L_CONF,"Freeing %s\n",pce->name);
	    free(*((char**)pce->var));
	}
	pce++;
    }
}

/**
 * Write the config specified in the web post to a file.  This
 * doesn't change the running config, just updates the config
 * file.
 *
 * \param pwsc the connection struct from the config-update url
 */
int config_write(WS_CONNINFO *pwsc) {
    FILE *configfile;
    char ctime_buf[27];
    time_t now;

    configfile=fopen(config.configfile,"w");
    if(!configfile)
	return -1;

    now=time(NULL);
    ctime_r(&now,ctime_buf);
    fprintf(configfile,"#\n# mt-daapd.conf\n#\n");
    fprintf(configfile,"# Edited: %s",ctime_buf);
    fprintf(configfile,"# By:     %s\n",ws_getvar(pwsc,"HTTP_USER"));
    fprintf(configfile,"#\n");

    fprintf(configfile,"web_root\t%s\n",ws_getvar(pwsc,"web_root"));
    fprintf(configfile,"port\t\t%s\n",ws_getvar(pwsc,"port"));
    fprintf(configfile,"admin_pw\t%s\n",ws_getvar(pwsc,"admin_pw"));
    fprintf(configfile,"mp3_dir\t\t%s\n",ws_getvar(pwsc,"mp3_dir"));
    fprintf(configfile,"servername\t%s\n",ws_getvar(pwsc,"servername"));
    fprintf(configfile,"runas\t\t%s\n",ws_getvar(pwsc,"runas"));
    fprintf(configfile,"playlist\t%s\n",ws_getvar(pwsc,"playlist"));
    if(ws_getvar(pwsc,"password") && strlen(ws_getvar(pwsc,"password")))
	fprintf(configfile,"password\t%s\n",ws_getvar(pwsc,"password"));
    fprintf(configfile,"extensions\t%s\n",ws_getvar(pwsc,"extensions"));
    fprintf(configfile,"db_dir\t\t%s\n",ws_getvar(pwsc,"db_dir"));
    fprintf(configfile,"rescan_interval\t%s\n",ws_getvar(pwsc,"rescan_interval"));
    fprintf(configfile,"scan_type\t%s\n",ws_getvar(pwsc,"scan_type"));
    if(ws_getvar(pwsc,"always_scan") && strlen(ws_getvar(pwsc,"always_scan")))
	fprintf(configfile,"always_scan\t%s\n",ws_getvar(pwsc,"always_scan"));
    if(ws_getvar(pwsc,"art_filename") && strlen(ws_getvar(pwsc,"art_filename")))
	fprintf(configfile,"art_filename\t%s\n",ws_getvar(pwsc,"art_filename"));
    if(ws_getvar(pwsc,"logfile") && strlen(ws_getvar(pwsc,"logfile")))
	fprintf(configfile,"logfile\t\t%s\n",ws_getvar(pwsc,"logfile"));
    fprintf(configfile,"process_m3u\t%s\n",ws_getvar(pwsc,"process_m3u"));    
    fprintf(configfile,"compress\t%s\n",ws_getvar(pwsc,"compress"));

    fclose(configfile);
    return 0;
}

/**
 * walk through a stream doing substitution on the
 * meta commands.  This is what fills in the fields
 * on the web page, or throws out the thread status
 * table on the status page.
 *
 * \param pwsc the web connection for the original page
 * \param fd_src the fd of the file to do substitutions on
 */
void config_subst_stream(WS_CONNINFO *pwsc, int fd_src) {
    int in_arg;
    char *argptr;
    char argbuffer[80];
    char next;
    CONFIGELEMENT *pce;
    char *first, *last;

    /* now throw out the file, with replacements */
    in_arg=0;
    argptr=argbuffer;

    while(1) {
	if(r_read(fd_src,&next,1) <= 0)
	    break;

	if(in_arg) {
	    if((next == '@') && (strlen(argbuffer) > 0)) {
		in_arg=0;

		DPRINTF(E_DBG,L_CONF,"Got directive %s\n",argbuffer);

		/* see if there are args */
		first=last=argbuffer;
		strsep(&last," ");

		pce=config_elements;
		while(pce->config_element != -1) {
		    if(strcasecmp(first,pce->name) == 0) {
			pce->emit(pwsc, pce->var,last);
			break;
		    }
		    pce++;
		}

		if(pce->config_element == -1) { /* bad subst */
		    ws_writefd(pwsc,"@%s@",argbuffer);
		}
	    } else if(next == '@') {
		ws_writefd(pwsc,"@");
		in_arg=0;
	    } else {
		if((argptr - argbuffer) < (sizeof(argbuffer)-1))
		    *argptr++ = next;
	    }
	} else {
	    if(next == '@') {
		argptr=argbuffer;
		memset(argbuffer,0,sizeof(argbuffer));
		in_arg=1;
	    } else {
		if(r_write(pwsc->fd,&next,1) == -1)
		    break;
	    }
	}
    }
}

/**
 * This is the webserver callback for all non-daap web requests.
 * This includes the admin page handling.
 *
 * \param pwsc web connection of request
 */
void config_handler(WS_CONNINFO *pwsc) {
    char path[PATH_MAX];
    char resolved_path[PATH_MAX];
    int file_fd;
    struct stat sb;
    char *pw;

    DPRINTF(E_DBG,L_CONF|L_WS,"Entering config_handler\n");

    config_set_status(pwsc,0,"Serving admin pages");
    
    pwsc->close=1;
    ws_addresponseheader(pwsc,"Connection","close");

    snprintf(path,PATH_MAX,"%s/%s",config.web_root,pwsc->uri);
    if(!realpath(path,resolved_path)) {
	pwsc->error=errno;
	DPRINTF(E_WARN,L_CONF|L_WS,"Cannot resolve %s\n",path);
	ws_returnerror(pwsc,404,"Not found");
	config_set_status(pwsc,0,NULL);
	return;
    }

    /* this should really return a 302:Found */
    stat(resolved_path,&sb);
    if(sb.st_mode & S_IFDIR)
	strcat(resolved_path,"/index.html");

    DPRINTF(E_DBG,L_CONF|L_WS,"Thread %d: Preparing to serve %s\n",
	    pwsc->threadno, resolved_path);

    if(strncmp(resolved_path,config.web_root,
	       strlen(config.web_root))) {
	pwsc->error=EINVAL;
	DPRINTF(E_WARN,L_CONF|L_WS,"Thread %d: Requested file %s out of root\n",
		pwsc->threadno,resolved_path);
	ws_returnerror(pwsc,403,"Forbidden");
	config_set_status(pwsc,0,NULL);
	return;
    }

    file_fd=r_open2(resolved_path,O_RDONLY);
    if(file_fd == -1) {
	pwsc->error=errno;
	DPRINTF(E_WARN,L_CONF|L_WS,"Thread %d: Error opening %s: %s\n",
		pwsc->threadno,resolved_path,strerror(errno));
	ws_returnerror(pwsc,404,"Not found");
	config_set_status(pwsc,0,NULL);
	return;
    }
    
    if(strcasecmp(pwsc->uri,"/config-update.html")==0) {
	/* don't update (and turn everything to (null)) the
	   configuration file if what the user's really trying to do is
	   stop the server */
	pw=ws_getvar(pwsc,"action");
	if(pw) {
	    /* ignore stopmdns and startmdns */
	    if (strcasecmp(pw,"stopdaap")==0) {
		config.stop=1;
	    }
	    if (strcasecmp(pw,"rescan")==0) {
		config.reload=1;
	    }
	} else {
	    /* we need to update stuff */
	    pw=ws_getvar(pwsc,"admin_pw");
	    if(pw) {
		if(config.adminpassword)
		    free(config.adminpassword);
		config.adminpassword=strdup(pw);
	    }

	    pw=ws_getvar(pwsc,"password");
	    if(pw) {
		if(config.readpassword)
		    free(config.readpassword);
		config.readpassword=strdup(pw);
	    }

	    pw=ws_getvar(pwsc,"rescan_interval");
	    if(pw) {
		config.rescan_interval=atoi(pw);
	    }

	    if(!config_file_is_readonly()) {
		DPRINTF(E_INF,L_CONF|L_WS,"Updating config file\n");
		config_write(pwsc);
	    }
	}
    }

    ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
    ws_emitheaders(pwsc);
    
    if(strcasecmp(&resolved_path[strlen(resolved_path) - 5],".html") == 0) {
	config_subst_stream(pwsc, file_fd);
    } else { 
	copyfile(file_fd,pwsc->fd);
    }

    r_close(file_fd);
    DPRINTF(E_DBG,L_CONF|L_WS,"Thread %d: Served successfully\n",pwsc->threadno);
    config_set_status(pwsc,0,NULL);
    return;
}

/**
 * The auth handler for the admin pages
 *
 * \param user username passed in the auth request
 * \param password password passed in the auth request
 */
int config_auth(char *user, char *password) {
    if((!password)||(!config.adminpassword))
	return 0;
    return !strcmp(password,config.adminpassword);
}


/**
 * Used to emit a string configuration value to an admin page
 *
 * \param pwsc web connection
 * \param value the variable that was requested
 * \param arg any args passwd with the meta command
 */
void config_emit_string(WS_CONNINFO *pwsc, void *value, char *arg) {
    if(*((char**)value))
	ws_writefd(pwsc,"%s",*((char**)value));
}

/**
 * Emit a string to the admin page.  This is an actual string,
 * not a pointer to a string pointer, like emit_string.
 *
 * \param pwsc web connection
 * \param value the variable that was requested
 * \param arg any args passwd with the meta command
 */
void config_emit_literal(WS_CONNINFO *pwsc, void *value, char *arg) {
    ws_writefd(pwsc,"%s",(char*)value);
}


/**
 * Emit an integer valut to the web page.
 *
 * \param pwsc web connection
 * \param value the variable that was requested
 * \param arg any args passwd with the meta command
 */
void config_emit_int(WS_CONNINFO *pwsc, void *value, char *arg) {
    ws_writefd(pwsc,"%d",*((int*)value));
}

/**
 * Dumps the status and control block to the web page.  This is the
 * page that offers the user the ability to rescan, or stop the
 * daap server.
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  This is
 *        required by the config struct, but unused.
 * \param arg any args passwd with the meta command.  Also unused
 */
void config_emit_service_status(WS_CONNINFO *pwsc, void *value, char *arg) {
    int mdns_running;
    char *html;
    char buf[256];
    int r_days, r_hours, r_mins, r_secs;
    int scanning;

    ws_writefd(pwsc,"<TABLE><TR><TH ALIGN=LEFT>Service</TH>");
    ws_writefd(pwsc,"<TH ALIGN=LEFT>Status</TH><TH ALIGN=LEFT>Control</TH></TR>\n");

    ws_writefd(pwsc,"<TR><TD>Rendezvous</TD>");
#ifndef WITHOUT_MDNS
    if(config.use_mdns) {
	mdns_running=!rend_running();

	if(mdns_running) {
	    html="<a href=\"config-update.html?action=stopmdns\">Stop MDNS Server</a>";
	} else {
	    html="<a href=\"config-update.html?action=startmdns\">Start MDNS Server</a>";
	}

	ws_writefd(pwsc,"<TD>%s</TD><TD>%s</TD></TR>\n",mdns_running ? "Running":"Stopped",
		   html);
    } else {
	ws_writefd(pwsc,"<TD>Not configured</TD><TD>&nbsp;</TD></TR>\n");
    }
#else
    ws_writefd(pwsc,"<TD>No Support</TD><TD>&nbsp;</TD></TR>\n");
#endif

    ws_writefd(pwsc,"<TR><TD>DAAP Server</TD><TD>%s</TD>",config.stop ? "Stopping":"Running");
    if(config.stop) {
	ws_writefd(pwsc,"<TD>Wait...</TD></TR>\n");
    } else {
	ws_writefd(pwsc,"<TD><a href=\"config-update.html?action=stopdaap\">Stop DAAP Server</a></TD></TR>");
    }

    scanning = config.reload;
    ws_writefd(pwsc,"<TR><TD>Background scanner</TD><TD>%s</TD>",scanning ? "Running":"Idle");
    if(scanning) {
	ws_writefd(pwsc,"<TD>Wait...</TD></TR>");
    } else {
	ws_writefd(pwsc,"<TD><A HREF=\"config-update.html?action=rescan\">Start Scan</A></TD></TR>");
    }

    ws_writefd(pwsc,"</TABLE>\n");

    ws_writefd(pwsc,"<TABLE>\n");
    ws_writefd(pwsc,"<TR>\n");
    ws_writefd(pwsc," <TH>Uptime</TH>\n");

    r_secs=time(NULL)-config.stats.start_time;

    r_days=r_secs/(3600 * 24);
    r_secs -= ((3600 * 24) * r_days);

    r_hours=r_secs/3600;
    r_secs -= (3600 * r_hours);

    r_mins=r_secs/60;
    r_secs -= 60 * r_mins;

    memset(buf,0x0,sizeof(buf));
    if(r_days) 
	sprintf((char*)&buf[strlen(buf)],"%d day%s, ", r_days,
		r_days == 1 ? "" : "s");

    if(r_days || r_hours) 
	sprintf((char*)&buf[strlen(buf)],"%d hour%s, ", r_hours,
		r_hours == 1 ? "" : "s");

    if(r_days || r_hours || r_mins)
	sprintf((char*)&buf[strlen(buf)],"%d minute%s, ", r_mins,
		r_mins == 1 ? "" : "s");

    sprintf((char*)&buf[strlen(buf)],"%d second%s ", r_secs,
	    r_secs == 1 ? "" : "s");
    
    ws_writefd(pwsc," <TD>%s</TD>\n",buf);
    ws_writefd(pwsc,"</TR>\n");
    
    ws_writefd(pwsc,"<TR>\n");
    ws_writefd(pwsc," <TH>Songs</TH>\n");
    ws_writefd(pwsc," <TD>%d</TD>\n",db_get_song_count());
    ws_writefd(pwsc,"</TR>\n");

    ws_writefd(pwsc,"<TR>\n");
    ws_writefd(pwsc," <TH>Songs Served</TH>\n");
    ws_writefd(pwsc," <TD>%d</TD>\n",config.stats.songs_served);
    ws_writefd(pwsc,"</TR>\n");

    if(!scanning) {
	ws_writefd(pwsc,"<TR>\n");
	ws_writefd(pwsc," <TH>DB Version</TH>\n");
	ws_writefd(pwsc," <TD>%d</TD>\n",db_version());
	ws_writefd(pwsc,"</TR>\n");
    }

    /*
    ws_writefd(pwsc,"<TR>\n");
    ws_writefd(pwsc," <TH>Bytes Served</TH>\n");
    ws_writefd(pwsc," <TD>%d</TD>\n",config.stats.songs_served);
    ws_writefd(pwsc,"</TR>\n");
    */

    ws_writefd(pwsc,"</TABLE>\n");
}


/**
 * Get the count of connected users.  This is actually not totally accurate,
 * as there may be a "connected" host that is in between requesting songs.
 * It's marginally close though.  It is really the number of connections
 * with non-zero session numbers.  
 *
 * \returns connected user count
 */
int config_get_session_count(void) {
    SCAN_STATUS *pcurrent, *pcheck;
    int count=0;

    if(config_mutex_lock()) {
	return 0;
    }

    pcurrent=scan_status.next;

    while(pcurrent) {
	pcheck=scan_status.next;
	while(pcheck != pcurrent) {
	    if(strcmp(pcheck->host,pcurrent->host) == 0)
		break;
	    pcheck=pcheck->next;
	}
	
	if(pcheck == pcurrent)
	    count++;

	pcurrent=pcurrent->next;
    }

    config_mutex_unlock();
    return count;
}


/**
 * dump the html code for the SESSION-COUNT command
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  This is
 *        required by the config struct, but unused.
 * \param arg any args passwd with the meta command.  Also unused
 */
void config_emit_session_count(WS_CONNINFO *pwsc, void *value, char *arg) {
    ws_writefd(pwsc,"%d",config_get_session_count());
}

/**
 * dump the html code for the THREADSTAT command
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  This is
 *        required by the config struct, but unused.
 * \param arg any args passwd with the meta command.  Also unused
 */
void config_emit_threadstatus(WS_CONNINFO *pwsc, void *value, char *arg) {
    SCAN_STATUS *pss;
    
    if(config_mutex_lock())
	return;

    ws_writefd(pwsc,"<TABLE><TR><TH ALIGN=LEFT>Thread</TH>");
    ws_writefd(pwsc,"<TH ALIGN=LEFT>Session</TH><TH ALIGN=LEFT>Host</TH>");
    ws_writefd(pwsc,"<TH ALIGN=LEFT>Action</TH></TR>\n");


    pss=scan_status.next;
    while(pss) {
	ws_writefd(pwsc,"<TR><TD>%d</TD><TD>%d</TD><TD>%s</TD><TD>%s</TD></TR>\n",
		   pss->thread,pss->session,pss->host,pss->what);
	pss=pss->next;
    }

    ws_writefd(pwsc,"</TABLE>\n");
    config_mutex_unlock();
}


/**
 * Kind of a hackish function to emit false or true if the served page is the
 * same name as the argument.  This is basically used to not make the current
 * tab a link when it is selected.  Kind of a hack.
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  Unused.
 * \param arg any args passwd with the meta command.  In this case
 * it is the page to test to see if it matches
 */
void config_emit_ispage(WS_CONNINFO *pwsc, void *value, char *arg) {
    char *first;
    char *last;

    char *page, *true, *false;

    DPRINTF(E_DBG,L_CONF|L_WS,"Splitting arg %s\n",arg);

    first=last=arg;
    strsep(&last,":");
    
    if(last) {
	page=strdup(first);
	if(!page)
	    return;
	first=last;
	strsep(&last,":");
	if(last) {
	    true=strdup(first);
	    false=strdup(last);
	    if((!true)||(!false))
		return;
	} else {
	    true=strdup(first);
	    if(!true)
		return;
	    false=NULL;
	}
    } else {
	return;
    }


    DPRINTF(E_DBG,L_CONF|L_WS,"page: %s, uri: %s\n",page,pwsc->uri);

    if((strlen(page) > strlen(pwsc->uri)) ||
       (strcasecmp(page,(char*)&pwsc->uri[strlen(pwsc->uri) - strlen(page)]) != 0)) {
	ws_writefd(pwsc,"%s",false);
    } else {
	ws_writefd(pwsc,"%s",true);
    }


    if(page)
	free(page);

    if(true)
	free(true);

    if(false)
	free(false);
}

/**
 * Emit the html for the USER command
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  Unused.
 * \param arg any args passwd with the meta command.  Unused.
 */
void config_emit_user(WS_CONNINFO *pwsc, void *value, char *arg) {
    if(ws_getvar(pwsc, "HTTP_USER")) {
	ws_writefd(pwsc,"%s",ws_getvar(pwsc, "HTTP_USER"));
    }
    return;
}

/**
 * Check to see if the config file is read only
 *
 * \returns 1 if the file is readonly, 0 otherwise
 */
int config_file_is_readonly(void) {
    FILE *fin;

    fin=fopen(config.configfile,"r+");
    if(!fin) {
	return 1;
    }

    fclose(fin);
    return 0;
}

/**
 * implement the READONLY command.  This is really a hack so
 * that the html config form isn't editable if the config file
 * isn't writable by the server.
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  Unused.
 * \param arg any args passwd with the meta command.  Unused
 */
void config_emit_readonly(WS_CONNINFO *pwsc, void *value, char *arg) {
    if(config_file_is_readonly()) {
	ws_writefd(pwsc,"READONLY");
    }
}

/**
 * implement the INCLUDE command.  This is essentially a server 
 * side include.
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  Unused.
 * \param arg any args passwd with the meta command.  Unused.
 */
void config_emit_include(WS_CONNINFO *pwsc, void *value, char *arg) {
    char resolved_path[PATH_MAX];
    char path[PATH_MAX];
    int file_fd;
    struct stat sb;

    DPRINTF(E_DBG,L_CONF|L_WS,"Preparing to include %s\n",arg);
    
    snprintf(path,PATH_MAX,"%s/%s",config.web_root,arg);
    if(!realpath(path,resolved_path)) {
	pwsc->error=errno;
	DPRINTF(E_WARN,L_CONF|L_WS,"Cannot resolve %s\n",path);
	ws_writefd(pwsc,"<hr><i>error: cannot find %s</i><hr>",arg);
	return;
    }

    /* this should really return a 302:Found */
    stat(resolved_path,&sb);
    if(sb.st_mode & S_IFDIR) {
	ws_writefd(pwsc,"<hr><i>error: cannot include director %s</i><hr>",arg);
	return;
    }


    DPRINTF(E_DBG,L_CONF|L_WS,"Thread %d: Preparing to serve %s\n",
	    pwsc->threadno, resolved_path);

    if(strncmp(resolved_path,config.web_root,
	       strlen(config.web_root))) {
	pwsc->error=EINVAL;
	DPRINTF(E_LOG,L_CONF|L_WS,"Thread %d: Requested file %s out of root\n",
		pwsc->threadno,resolved_path);
	ws_writefd(pwsc,"<hr><i>error: %s out of web root</i><hr>",arg);
	return;
    }

    file_fd=r_open2(resolved_path,O_RDONLY);
    if(file_fd == -1) {
	pwsc->error=errno;
	DPRINTF(E_LOG,L_CONF|L_WS,"Thread %d: Error opening %s: %s\n",
		pwsc->threadno,resolved_path,strerror(errno));
	ws_writefd(pwsc,"<hr><i>error: cannot open %s: %s</i><hr>",arg,strerror(errno));
	return;
    }
    
    config_subst_stream(pwsc, file_fd);

    r_close(file_fd);
    DPRINTF(E_DBG,L_CONF|L_WS,"Thread %d: included successfully\n",pwsc->threadno);
    return;
}

/**
 * Update the status info for a particuarl thread.  The thread
 * status is the string displayed in the THREADSTAT block of the
 * admin page.  That string is set with the function, which take
 * a printf-style format specifier.  Setting the status to NULL
 * will remove the thread from the config table.
 *
 * \param pwsc the web connection of the thread to update
 * \param session the session id of that thread
 * \param fmt printf style form specifier
 */
void config_set_status(WS_CONNINFO *pwsc, int session, char *fmt, ...) {
    char buffer[1024];
    va_list ap;
    SCAN_STATUS *pfirst, *plast;

    DPRINTF(E_DBG,L_CONF,"Entering config_set_status\n");

    if(config_mutex_lock()) {
	/* we should really shutdown the app here... */
	DPRINTF(E_FATAL,L_CONF,"Error acquiring config mutex\n");
    }

    pfirst=plast=scan_status.next;
    while((pfirst) && (pfirst->thread != pwsc->threadno)) {
	plast=pfirst;
	pfirst=pfirst->next;
    }

    if(fmt) {
	va_start(ap, fmt);
	vsnprintf(buffer, 1024, fmt, ap);
	va_end(ap);

	if(pfirst) { /* already there */
	    free(pfirst->what);
	    pfirst->what = strdup(buffer);
	    pfirst->session = session; /* this might change! */
	} else {
	    pfirst=(SCAN_STATUS*)malloc(sizeof(SCAN_STATUS));
	    if(pfirst) {
		pfirst->what = strdup(buffer);
		pfirst->session = session;
		pfirst->thread = pwsc->threadno;
		pfirst->next = scan_status.next;
		pfirst->host = strdup(pwsc->hostname);
		scan_status.next=pfirst;
	    }
	}
    } else {
	if(!pfirst) {
	    config_mutex_unlock();
	    DPRINTF(E_DBG,L_CONF,"Exiting config_set_status\n");
	    return;
	}

	if(pfirst==plast) { 
	    scan_status.next=pfirst->next;
	    free(pfirst->what);
	    free(pfirst->host);
	    free(pfirst);
	} else {
	    plast->next = pfirst->next;
	    free(pfirst->what);
	    free(pfirst->host);
	    free(pfirst);
	}
    }

    config_mutex_unlock();
    DPRINTF(E_DBG,L_CONF,"Exiting config_set_status\n");
}

/**
 * Lock the config mutex.  This is the mutex that provides
 * thread safety around the threadstat list.
 */
int config_mutex_lock(void) {
    int err;

    if((err=pthread_mutex_lock(&scan_mutex))) {
	errno=err;
	return - 1;
    }

    return 0;
}

/**
 * Unlock the config mutex.  This is the mutex that provides
 * thread safety for the threadstat list
 */
int config_mutex_unlock(void) {
    int err;

    if((err=pthread_mutex_unlock(&scan_mutex))) {
	errno=err;
	return -1;
    }

    return 0;
}

/**
 * Get the next available session id.
 *
 * \returns duh... the next available session id
 */
int config_get_next_session(void) {
    int session;
    config_mutex_lock();

    session=++config_session;
    config_mutex_unlock();

    return session;
}

/**
 * implement the VERSION command
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  Unused.
 * \param arg any args passwd with the meta command.  Unused.
 */
void config_emit_version(WS_CONNINFO *pwsc, void *value, char *arg) {
    ws_writefd(pwsc,"%s",VERSION);
}


/**
 * implement the SYSTEM command.
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  Unused.
 * \param arg any args passwd with the meta command.  Unused.
 */
void config_emit_system(WS_CONNINFO *pwsc, void *value, char *arg) {
    ws_writefd(pwsc,"%s","Ralink");
}


/**
 * Implement the FLAGS command.
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  Unused.
 * \param arg any args passwd with the meta command.  Unused.
 */
void config_emit_flags(WS_CONNINFO *pwsc, void *value, char *arg) {
#ifdef OGGVORBIS
    ws_writefd(pwsc,"%s ","--enable-oggvorbis");
#endif

#ifdef WITH_GDBM
    ws_writefd(pwsc,"%s ","--with-gdbm");
#endif

#ifdef WITH_HOWL
    ws_writefd(pwsc,"%s ","--enable-howl");
#endif

#ifdef NSLU2
    ws_writefd(pwsc,"%s ","--enable-nslu2");
#endif
}
