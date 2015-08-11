/*
 * $Id: configfile.c 1640 2007-08-27 00:57:47Z rpedde $
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
#  include "config.h"
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include <sys/stat.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#include "daapd.h"
#include "conf.h"
#include "configfile.h"
#include "db-generic.h"
#include "err.h"
#include "os.h"
#include "restart.h"
#include "xml-rpc.h"
#include "upnp.h"

#ifndef WITHOUT_MDNS
# include "rend.h"
#endif


/*
 * Forwards
 */
static void config_emit_literal(WS_CONNINFO *pwsc, void *value, char *arg);
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
static void config_emit_conffile(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_host(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_config(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_servername(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_emit_upnp(WS_CONNINFO *pwsc, void *value, char *arg);
static void config_subst_stream(WS_CONNINFO *pwsc, IOHANDLE hfile);
static void config_content_type(WS_CONNINFO *pwsc, char *path);

/*
 * Defines
 */
#define CONFIG_TYPE_INT       0  /**< Config element is an integer */
#define CONFIG_TYPE_STRING    1  /**< Config element is a string */
#define CONFIG_TYPE_SPECIAL   4  /**< Config element is a web  parameter */

typedef struct tag_contenttypes {
    char *extension;
    char *contenttype;
    int decache;
} CONTENTTYPES;

CONTENTTYPES config_default_types[] = {
    { ".css", "text/css; charset=utf-8", 0 },
    { ".txt", "text/plain; charset=utf-8", 0 },
    { ".xml", "text/xml; charset=utf-8", 1 },
    { NULL, NULL }
};

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
    /*
      { 1,1,0,CONFIG_TYPE_STRING,"runas",(void*)&config.runas,config_emit_string },
      { 1,1,0,CONFIG_TYPE_STRING,"web_root",(void*)&config.web_root,config_emit_string },
      { 1,1,0,CONFIG_TYPE_INT,"port",(void*)&config.port,config_emit_int },
      { 1,1,0,CONFIG_TYPE_STRING,"admin_pw",(void*)&config.adminpassword,config_emit_string },
      { 1,1,0,CONFIG_TYPE_STRING,"mp3_dir",(void*)&config.mp3dir,config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"db_dir",(void*)&config.dbdir,config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"db_type",(void*)&config.dbtype,config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"db_parms",(void*)&config.dbparms,config_emit_string },
      { 1,0,0,CONFIG_TYPE_INT,"debuglevel",(void*)NULL,config_emit_debuglevel },
      { 1,1,0,CONFIG_TYPE_STRING,"servername",(void*)&config.servername,config_emit_string },
      { 1,0,0,CONFIG_TYPE_INT,"rescan_interval",(void*)&config.rescan_interval,config_emit_int },
      { 1,0,0,CONFIG_TYPE_INT,"always_scan",(void*)&config.always_scan,config_emit_int },
      { 1,0,0,CONFIG_TYPE_INT,"latin1_tags",(void*)&config.latin1_tags,config_emit_int },
      { 1,0,0,CONFIG_TYPE_INT,"process_m3u",(void*)&config.process_m3u,config_emit_int },
      { 1,0,0,CONFIG_TYPE_INT,"scan_type",(void*)&config.scan_type,config_emit_int },
      { 1,0,0,CONFIG_TYPE_INT,"compress",(void*)&config.compress,config_emit_int },
      { 1,0,0,CONFIG_TYPE_STRING,"playlist",(void*)&config.playlist,config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"extensions",(void*)&config.extensions,config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"interface",(void*)&config.iface,config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"ssc_codectypes",(void*)&config.ssc_codectypes,config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"ssc_prog",(void*)&config.ssc_prog,config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"password",(void*)&config.readpassword, config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"compdirs",(void*)&config.compdirs, config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"logfile",(void*)&config.logfile, config_emit_string },
      { 1,0,0,CONFIG_TYPE_STRING,"art_filename",(void*)&config.artfilename,config_emit_string },
    */
    { 0,0,0,CONFIG_TYPE_SPECIAL,"servername",(void*)NULL,config_emit_servername },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"config",(void*)NULL,config_emit_config },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"conffile",(void*)NULL,config_emit_conffile },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"host",(void*)NULL,config_emit_host },
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
    { 0,0,0,CONFIG_TYPE_SPECIAL,"flags",(void*)NULL,config_emit_flags },
    { 0,0,0,CONFIG_TYPE_SPECIAL,"upnp",(void*)NULL,config_emit_upnp },
    { -1,1,0,CONFIG_TYPE_STRING,NULL,NULL,NULL }
};

int config_session=0;                                   /**< session counter */

#define MAX_LINE 1024


int config_password_required(WS_CONNINFO *pwsc, char *role) {
    char *pw;

    DPRINTF(E_DBG,L_MISC,"Checking if pw required for %s as %s\n",
            ws_uri(pwsc), role);

    /* hack hack hack - this needs moved into the upnp plugin */
    if(strncasecmp(ws_uri(pwsc),"/upnp",5) == 0) {
        DPRINTF(E_DBG,L_MISC,"Nope\n");
        return FALSE;
    }

    if(strcasecmp(role,"admin") == 0) {
        pw = conf_alloc_string("general","admin_pw",NULL);
        if((!pw) || ((pw) && (!strlen(pw)))) {
            /* don't need a password from localhost
               when the password isn't set */

#ifdef WIN32
            /* except on windows */
            return FALSE;
#endif

            if((ws_hostname(pwsc)) && (os_islocaladdr(ws_hostname(pwsc)))) {
                if(pw) free(pw);
                return FALSE;
            }
        }
        if(pw) free(pw);
        DPRINTF(E_DBG,L_MISC,"Yep\n");

        return TRUE;
    }

    if(!strcasecmp(role,"user")) {
        pw = conf_alloc_string("general","password",NULL);
        if(pw && strlen(pw)) {
            DPRINTF(E_DBG,L_MISC,"Yep: %s\n",pw);
            free(pw);
            return TRUE;
        }
        if(pw) free(pw);
        DPRINTF(E_DBG,L_MISC,"Nope\n");
        return FALSE;
    }

    DPRINTF(E_LOG,L_MISC,"Bad role type for auth: %s\n",role);
    return TRUE; /* other class */
}


/**
 * used in auth handlers to see if the username and password
 * are sufficient to grant a specific role.  The roles "user" and
 * "admin" are built-in roles, and are the roles used to control access
 * to the music and the admin pages respectively.
 */
int config_matches_role(WS_CONNINFO *pwsc, char *username,
                        char *password, char *role) {
    char *required_pw;
    int result;

    /* sanity check */
    if((strcasecmp(role,"admin") != 0) &&
       (strcasecmp(role,"user") !=0)) {
        DPRINTF(E_LOG,L_MISC,"Unknown role: %s\n",role);
        return FALSE;
    }

    if(password == NULL)
        return config_password_required(pwsc,role) ? FALSE : TRUE;

    /* if we have admin auth, we have everything */
    required_pw = conf_alloc_string("general","admin_pw",NULL);
    if((required_pw) && (strcmp(required_pw, password)==0)) {
        free(required_pw);
        return TRUE;
    }

    if(required_pw)
        free(required_pw);

    if(strcasecmp(role,"admin") == 0) {
        DPRINTF(E_LOG,L_MISC,"Auth: failed attempt to gain admin privs by "
                "%s from %s\n",username, ws_hostname(pwsc));
        return FALSE;
    }

    /* we're checking for user privs */
    required_pw = conf_alloc_string("general","password",NULL);

    if(!required_pw) /* none set */
        return TRUE;

    result = FALSE;
    if(strcmp(required_pw,password) == 0)
        result = TRUE;

    free(required_pw);
    if(!result) {
        DPRINTF(E_LOG,L_MISC,"Auth: failed attempt to gain user privs by "
                "%s from %s\n",ws_hostname(pwsc), username);
    }
    return result;
}

/**
 * set the content type based on the file type
 */
void config_content_type(WS_CONNINFO *pwsc, char *path) {
    char *extension;
    CONTENTTYPES *pct=config_default_types;

    extension=strrchr(path,'.');
    if(extension) {
        while((pct->extension) && (strcasecmp(pct->extension,extension)))
            pct++;

        if(pct->extension) {
            ws_addresponseheader(pwsc,"Content-Type",pct->contenttype);
        }

        if(pct->decache) {
            ws_addresponseheader(pwsc,"Cache-Control","no-cache");
            ws_addresponseheader(pwsc,"Expires","-1");
        }
    }
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
void config_subst_stream(WS_CONNINFO *pwsc, IOHANDLE hfile) {
    int in_arg;
    char *argptr;
    char argbuffer[256];
    char next;
    CONFIGELEMENT *pce;
    char *first, *last;

    char outbuffer[1024];
    int out_index;
    uint32_t bytes_read;

    /* now throw out the file, with replacements */
    in_arg=0;
    argptr=argbuffer;
    out_index=0;

    while(1) {
        /* FIXME: use a reasonable sized buffer, or implement buffered io */
        bytes_read = 1;
        if((!io_read(hfile,(unsigned char *)&next,&bytes_read)) || !bytes_read)
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

                /* flush whatever is in the buffer */
                if(out_index) {
                    ws_writebinary(pwsc,outbuffer,out_index);
                    out_index=0;
                }
            } else {
                outbuffer[out_index] = next;
                out_index++;

                if(out_index == sizeof(outbuffer)) {
                    ws_writebinary(pwsc,outbuffer,out_index);
                    out_index=0;
                }
            }
        }
    }
    if(out_index) {
        ws_writebinary(pwsc,outbuffer,out_index);
        out_index=0;
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
    char web_root[PATH_MAX];
    IOHANDLE hfile;
    struct stat sb;
    char *pw;
    int size;

    if(!ws_hostname(pwsc)) {
        ws_returnerror(pwsc,500,"Couldn't determine remote hostname");
        ws_should_close(pwsc,1);
        return;
    }

    /* FIXME: should feed this file directly, so as to bypass perms */
    if(!os_islocaladdr(ws_hostname(pwsc))) {
        pw=conf_alloc_string("general","admin_pw",NULL);
        if((!pw) || (strlen(pw) == 0)) {
            if(pw) free(pw);
            /* if we aren't getting the no_access.html page, we should */
            if(strcmp(ws_uri(pwsc),"/no_access.html") != 0) {
                ws_addresponseheader(pwsc,"location","/no_access.html");
                ws_returnerror(pwsc,302,"Moved Temporarily");
                ws_should_close(pwsc,1);
                return;
            }
        }
    }

    size = sizeof(web_root);
    if(conf_get_string("general","web_root",NULL,web_root,
                       &size) == CONF_E_NOTFOUND) {
        DPRINTF(E_FATAL,L_WS,"No web root!\n");
    }

    DPRINTF(E_DBG,L_CONF|L_WS,"Entering config_handler\n");

    config_set_status(pwsc,0,"Serving admin pages");

    ws_should_close(pwsc,1);
    ws_addresponseheader(pwsc,"Connection","close");

    if(strcasecmp(ws_uri(pwsc),"/xml-rpc")==0) {
        // perhaps this should get a separate handler
        config_set_status(pwsc,0,"Serving xml-rpc method");
        xml_handle(pwsc);
        DPRINTF(E_DBG,L_CONF|L_XML,"Thread %d: xml-rpc served\n",ws_threadno(pwsc));
        config_set_status(pwsc,0,NULL);
        return;
    }

    if(('/' == web_root[strlen(web_root) - 1]) ||
       ('\\' == web_root[strlen(web_root) - 1])) {
        web_root[strlen(web_root) - 1] = '\0';
    }

    snprintf(path,PATH_MAX,"%s/%s",web_root,ws_uri(pwsc));
    DPRINTF(E_DBG,L_CONF|L_WS,"Realpathing %s\n",path);
    if(!realpath(path,resolved_path)) {
        pwsc->error=errno;
        DPRINTF(E_WARN,L_CONF|L_WS,"Cannot resolve %s\n",path);
        ws_returnerror(pwsc,404,"Not found");
        config_set_status(pwsc,0,NULL);
        return;
    }

    /* this is quite broken, but will work */
    os_stat(resolved_path,&sb);
    if(S_ISDIR(sb.st_mode)) {
        ws_addresponseheader(pwsc,"Location","index.html");
        ws_returnerror(pwsc,302,"Moved");
        config_set_status(pwsc,0,NULL);
        ws_should_close(pwsc,1);
        return;
    }

    DPRINTF(E_DBG,L_CONF|L_WS,"Thread %d: Preparing to serve %s\n",
            ws_threadno(pwsc), resolved_path);

    if(strncmp(resolved_path,web_root,strlen(web_root))) {
        pwsc->error=EINVAL;
        DPRINTF(E_WARN,L_CONF|L_WS,"Thread %d: Requested file %s out of root\n",
                ws_threadno(pwsc),resolved_path);
        ws_returnerror(pwsc,403,"Forbidden");
        config_set_status(pwsc,0,NULL);
        return;
    }

    hfile = io_new();
    if(!hfile) {
        DPRINTF(E_FATAL,L_CONF | L_WS,"Cannot create file handle\n");
        exit(-1);
    }

    DPRINTF(E_DBG,L_CONF|L_WS,"Opening %s\n",resolved_path);
    if(!io_open(hfile,"file://%U",resolved_path)) {
        ws_set_err(pwsc,E_WS_NATIVE);
        DPRINTF(E_WARN,L_CONF|L_WS,"Thread %d: Error opening %s: %s\n",
                ws_threadno(pwsc),resolved_path,io_errstr(hfile));
        ws_returnerror(pwsc,404,"Not found");
        config_set_status(pwsc,0,NULL);
        io_dispose(hfile);
        return;
    }

    /* FIXME: use xml-rpc for all these */
    if(strcasecmp(ws_uri(pwsc),"/config-update.html")==0) {
        /* don't update (and turn everything to (null)) the
           configuration file if what the user's really trying to do is
           stop the server */
        pw=ws_getvar(pwsc,"action");
        if(pw) {
            /* ignore stopmdns and startmdns */
            if (strcasecmp(pw,"stopdaap")==0) {
                config.stop=1;
            } else if (strcasecmp(pw,"rescan")==0) {
                config.reload=1;
            }
        }
    }

    config_content_type(pwsc, resolved_path);

    ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
    ws_emitheaders(pwsc);

    if((strcasecmp(&resolved_path[strlen(resolved_path) - 5],".html") == 0) ||
       (strcasecmp(&resolved_path[strlen(resolved_path) - 4],".xml") == 0)) {
        config_subst_stream(pwsc, hfile);
    } else {
        ws_copyfile(pwsc,hfile,NULL);
    }

    io_close(hfile);
    io_dispose(hfile);

    DPRINTF(E_DBG,L_CONF|L_WS,"Thread %d: Served successfully\n",ws_threadno(pwsc));
    config_set_status(pwsc,0,NULL);
    return;
}

/**
 * The auth handler for the admin pages
 *
 * \param user username passed in the auth request
 * \param password password passed in the auth request
 */
int config_auth(WS_CONNINFO *pwsc, char *user, char *password) {
    return config_matches_role(pwsc,user,password,"admin");
}

void config_emit_upnp(WS_CONNINFO *pwsc, void *value, char *arg) {
#ifdef UPNP
    ws_writefd(pwsc,"%s",upnp_uuid());
#endif
}


/**
 * write the current servername
 */
void config_emit_servername(WS_CONNINFO *pwsc, void *value, char *arg) {
    char *servername = conf_get_servername();

    ws_writefd(pwsc,"%s",servername);
    free(servername);
}

/**
 * write a config value
 */
void config_emit_config(WS_CONNINFO *pwsc, void *value, char *arg) {
    char *new_arg = strdup(arg);
    char *section = new_arg;
    char *key;
    char *result_value;

    if(!new_arg)
        return;

    key = strchr(new_arg,'/');
    if(!key)
        return;

    *key = '\0';
    key++;

    result_value = conf_alloc_string(section, key, NULL);
    if(!result_value)
        ws_writefd(pwsc,"NULL");
    else {
        ws_writefd(pwsc,"%s",result_value);
        free(result_value);
    }
}

/**
 * emit the host value passed by the client web server.  This
 * is really used to autoconfig the java client
 *
 * \param pwsc web connection
 * \param value the variable that was requested
 * \param arg any args passwd with the meta command
 */
void config_emit_host(WS_CONNINFO *pwsc, void *value, char *arg) {
    char *host;
    char *port;
    char hostname[256];

    if(ws_getrequestheader(pwsc,"host")) {
        host = strdup(ws_getrequestheader(pwsc,"host"));
        if((port = strrchr(host,':'))) {
            *port = '\0';
        }
        ws_writefd(pwsc,"%s",host);
        free(host);
    } else {
        DPRINTF(E_LOG,L_CONF,"Didn't get a host header!\n");
        gethostname(hostname,sizeof(hostname));
        ws_writefd(pwsc,hostname);
    }

    return;
}


/**
 * emit the path to the current config file
 *
 * \param pwsc web connection
 * \param value the variable that was requested
 * \param arg any args passwd with the meta command
 */
void config_emit_conffile(WS_CONNINFO *pwsc, void *value, char *arg) {
    char fullpath[PATH_MAX];

    realpath(conf_get_filename(),fullpath);
    ws_writefd(pwsc,"%s",fullpath);
    return;
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
#ifndef WITHOUT_MDNS
    int mdns_running;
    char *html;
#endif
    char buf[256];
    int r_days, r_hours, r_mins, r_secs;
    int scanning;
    int song_count;

    ws_writefd(pwsc,"<table><tr><th align=\"left\">Service</th>");
    ws_writefd(pwsc,"<th align=\"left\">Status</th><th align=\"left\">Control</th></tr>\n");

    ws_writefd(pwsc,"<tr><td>Rendezvous</td>");
#ifndef WITHOUT_MDNS
    if(config.use_mdns) {
        mdns_running=!rend_running();

        if(mdns_running) {
            html="<a href=\"config-update.html?action=stopmdns\">Stop MDNS Server</a>";
        } else {
            html="<a href=\"config-update.html?action=startmdns\">Start MDNS Server</a>";
        }

        ws_writefd(pwsc,"<td>%s</td><td>%s</td></tr>\n",mdns_running ? "Running":"Stopped",
                   html);
    } else {
        ws_writefd(pwsc,"<td>Not configured</td><td>&nbsp;</td></tr>\n");
    }
#else
    ws_writefd(pwsc,"<td>No Support</td><td>&nbsp;</td></tr>\n");
#endif

    ws_writefd(pwsc,"<tr><td>DAAP Server</td><td>%s</td>",config.stop ? "Stopping":"Running");
    if(config.stop) {
        ws_writefd(pwsc,"<td>Wait...</td></tr>\n");
    } else {
        ws_writefd(pwsc,"<td><a href=\"config-update.html?action=stopdaap\">Stop DAAP Server</a></td></tr>");
    }

    scanning = config.reload;
    ws_writefd(pwsc,"<tr><td>Background scanner</td><td>%s</td>",scanning ? "Running":"Idle");
    if(scanning) {
        ws_writefd(pwsc,"<td>Wait...</td></tr>");
    } else {
        ws_writefd(pwsc,"<td><a href=\"config-update.html?action=rescan\">Start Scan</a></td></tr>");
    }

    ws_writefd(pwsc,"</table>\n");

    ws_writefd(pwsc,"<br />\n");

    ws_writefd(pwsc,"<table>\n");
    ws_writefd(pwsc,"<tr>\n");
    ws_writefd(pwsc," <th>Uptime</th>\n");

    r_secs=(int)(time(NULL)-config.stats.start_time);

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

    ws_writefd(pwsc," <td>%s</td>\n",buf);
    ws_writefd(pwsc,"</tr>\n");

    ws_writefd(pwsc,"<tr>\n");
    ws_writefd(pwsc," <th>Songs</th>\n");
    db_get_song_count(NULL,&song_count);
    ws_writefd(pwsc," <td>%d</td>\n",song_count);
    ws_writefd(pwsc,"</tr>\n");

    ws_writefd(pwsc,"<tr>\n");
    ws_writefd(pwsc," <th>Songs Served</th>\n");
    ws_writefd(pwsc," <td>%d</td>\n",config.stats.songs_served);
    ws_writefd(pwsc,"</tr>\n");

    if(!scanning) {
        ws_writefd(pwsc,"<tr>\n");
        ws_writefd(pwsc," <th>DB Version</th>\n");
        ws_writefd(pwsc," <td>%d</td>\n",db_revision());
        ws_writefd(pwsc,"</tr>\n");
    }

    /*
      ws_writefd(pwsc,"<tr>\n");
      ws_writefd(pwsc," <th>Bytes Served</th>\n");
      ws_writefd(pwsc," <td>%d</td>\n",config.stats.songs_served);
      ws_writefd(pwsc,"</tr>\n");
    */

    ws_writefd(pwsc,"</table>\n");
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
    WSTHREADENUM wste;
    WS_CONNINFO *pwsc;
    int count=0;

    pwsc = ws_thread_enum_first(config.server,&wste);
    while(pwsc) {
        count++;
        pwsc = ws_thread_enum_next(config.server,&wste);
    }

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
    WS_CONNINFO *pci;
    SCAN_STATUS *pss;
    WSTHREADENUM wste;

    ws_writefd(pwsc,"<table><tr><th align=\"left\">Thread</th>");
    ws_writefd(pwsc,"<th align=\"left\">Session</th><th align=\"left\">Host</th>");
    ws_writefd(pwsc,"<th align=\"left\">Action</th></tr>\n");


    pci = ws_thread_enum_first(config.server,&wste);
    while(pci) {
        pss = ws_get_local_storage(pci);
        if(pss) {
            ws_writefd(pwsc,"<tr><td>%d</td><td>%d</td><td>%s</td><td>%s</td></tr>\n",
                       pss->thread,pss->session,pss->host,pss->what ? pss->what : "Idle");
        }
        pci=ws_thread_enum_next(config.server,&wste);
    }

    ws_writefd(pwsc,"</table>\n");
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


    DPRINTF(E_DBG,L_CONF|L_WS,"page: %s, uri: %s\n",page,ws_uri(pwsc));

    if((strlen(page) > strlen(ws_uri(pwsc))) ||
       (strcasecmp(page,(char*)&ws_uri(pwsc)[strlen(ws_uri(pwsc)) - strlen(page)]) != 0)) {
        if(false) {
            ws_writefd(pwsc,"%s",false);
        }
    } else {
        if(true) {
            ws_writefd(pwsc,"%s",true);
        }
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
 * implement the READONLY command.  This is really a hack so
 * that the html config form isn't editable if the config file
 * isn't writable by the server.
 *
 * \param pwsc web connection
 * \param value the variable that was requested.  Unused.
 * \param arg any args passwd with the meta command.  Unused
 */
void config_emit_readonly(WS_CONNINFO *pwsc, void *value, char *arg) {
    if(!conf_iswritable()) {
        ws_writefd(pwsc,"readonly=\"readonly\"");
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
    char web_root[PATH_MAX];
    IOHANDLE hfile;
    struct stat sb;
    int size;

    size = sizeof(web_root);
    if(conf_get_string("general","web_root",NULL,web_root,
                       &size) == CONF_E_NOTFOUND) {
        DPRINTF(E_FATAL,L_WS,"No web root!\n");
    }

    DPRINTF(E_DBG,L_CONF|L_WS,"Preparing to include %s\n",arg);

    if(('/' == web_root[strlen(web_root) - 1]) ||
       ('\\' == web_root[strlen(web_root) - 1])) {
        web_root[strlen(web_root) - 1] = '\0';
    }

    snprintf(path,PATH_MAX,"%s/%s",web_root,arg);
    if(!realpath(path,resolved_path)) {
        pwsc->error=errno;
        DPRINTF(E_WARN,L_CONF|L_WS,"Cannot resolve %s\n",path);
        ws_writefd(pwsc,"<hr><i>error: cannot find %s</i><hr>",arg);
        return;
    }

    /* this should really return a 302:Found */
    os_stat(resolved_path,&sb);
    if(sb.st_mode & S_IFDIR) {
        ws_writefd(pwsc,"<hr><i>error: cannot include dir %s</i><hr>",arg);
        return;
    }


    DPRINTF(E_DBG,L_CONF|L_WS,"Thread %d: Preparing to serve %s\n",
            ws_threadno(pwsc), resolved_path);

    if(strncmp(resolved_path,web_root,strlen(web_root))) {
        pwsc->error=EINVAL;
        DPRINTF(E_LOG,L_CONF|L_WS,"Thread %d: Requested file %s out of root\n",
                ws_threadno(pwsc),resolved_path);
        ws_writefd(pwsc,"<hr><i>error: %s out of web root</i><hr>",arg);
        return;
    }

    hfile = io_new();
    if(!hfile) {
        DPRINTF(E_LOG,L_CONF | L_WS,"Thread %d: Cannot create file handle\n",
                ws_threadno(pwsc));
        return;
    }
    if(!io_open(hfile,"file://%U",resolved_path)) {
        ws_set_err(pwsc,E_WS_NATIVE);
        DPRINTF(E_LOG,L_CONF|L_WS,"Thread %d: Error opening %s: %s\n",
                ws_threadno(pwsc),resolved_path,io_errstr(pwsc));
        ws_writefd(pwsc,"<hr><i>error: cannot open %s: %s</i><hr>",arg,
                   io_errstr(pwsc));
        io_dispose(hfile);
        return;
    }

    config_subst_stream(pwsc, hfile);

    io_close(hfile);
    io_dispose(hfile);
    DPRINTF(E_DBG,L_CONF|L_WS,"Thread %d: included successfully\n",ws_threadno(pwsc));
    return;
}

/**
 * free a SCAN_STATUS block
 *
 * @param vp pointer to SCAN_STATUS block
 */
void config_freescan(void *vp) {
    SCAN_STATUS *pss = (SCAN_STATUS*)vp;

    if(pss) {
        if(pss->what)
            free(pss->what);
        if(pss->host)
            free(pss->host);
        free(pss);
    }
}

/**
 * Update the status info for a particular thread.  The thread
 * status is the string displayed in the THREADSTAT block of the
 * admin page.  That string is set with the function, which take
 * a printf-style format specifier.
 *
 * \param pwsc the web connection of the thread to update
 * \param session the session id of that thread
 * \param fmt printf style form specifier
 */
void config_set_status(WS_CONNINFO *pwsc, int session, char *fmt, ...) {
    char buffer[1024];
    va_list ap;
    SCAN_STATUS *pfirst;
    char *newmsg = NULL;

    DPRINTF(E_DBG,L_CONF,"Entering config_set_status\n");

    if(fmt) {
        va_start(ap, fmt);
        vsnprintf(buffer, 1024, fmt, ap);
        va_end(ap);

        newmsg = strdup(buffer);
    }

    ws_lock_local_storage(pwsc);
    if(!(pfirst = ws_get_local_storage(pwsc))) {
        /* new info */
        pfirst=(SCAN_STATUS*)malloc(sizeof(SCAN_STATUS));
        if(pfirst) {
            pfirst->what = NULL;
            pfirst->thread = ws_threadno(pwsc);
            pfirst->host = strdup(ws_hostname(pwsc));
            ws_set_local_storage(pwsc,pfirst,config_freescan);
        } else {
            DPRINTF(E_FATAL,L_CONF,"Malloc Error\n");
        }
    }

    if(pfirst) {
        /* just update */
        if(pfirst->what) {
            free(pfirst->what);
        }
        pfirst->what=newmsg;
        pfirst->session=session;
    } else {
        if(newmsg)
            free(newmsg);
    }

    ws_unlock_local_storage(pwsc);
    DPRINTF(E_DBG,L_CONF,"Exiting config_set_status\n");
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
    ws_writefd(pwsc,"%s",HOST);
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

#ifdef FLAC
    ws_writefd(pwsc,"%s ","--enable-flac");
#endif

#ifdef MUSEPACK
    ws_writefd(pwsc,"%s ","--enable-musepack");
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
