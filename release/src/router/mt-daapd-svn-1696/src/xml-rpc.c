/*
 * $Id: xml-rpc.c 1690 2007-10-23 04:23:50Z rpedde $
 *
 * This really isn't xmlrpc.  It's xmlrpc-ish.  Emphasis on -ish.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif
#include <errno.h>
#include <limits.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef WIN32
#include <io.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include "daapd.h"

#include "configfile.h"
#include "conf.h"
#include "db-generic.h"
#include "err.h"
#include "mp3-scanner.h"
#include "os.h"
#include "plugin.h"
#include "rend.h"
#include "webserver.h"
#include "xml-rpc.h"

/* typedefs */
typedef struct tag_xmlstack {
    char *tag;
    struct tag_xmlstack *next;
} XMLSTACK;

struct tag_xmlstruct {
    WS_CONNINFO *pwsc;
    int stack_level;
    XMLSTACK stack;
};

/* Forwards */
void xml_get_stats(WS_CONNINFO *pwsc);
void xml_set_config(WS_CONNINFO *pwsc);
void xml_browse_path(WS_CONNINFO *pwsc);
void xml_rescan(WS_CONNINFO *pwsc);
void xml_return_error(WS_CONNINFO *pwsc, int err, char *errstr);
char *xml_entity_encode(char *original);

void xml_return_error(WS_CONNINFO *pwsc, int err, char *errstr) {
    XMLSTRUCT *pxml;

    pxml=xml_init(pwsc,TRUE);
    xml_push(pxml,"results");

    xml_output(pxml,"status","%d",err);
    xml_output(pxml,"statusstring","%s",errstr);

    xml_pop(pxml); /* results */
    xml_deinit(pxml);
    return;
}


/**
 * create an xml response structure, a helper struct for
 * building xml responses.
 *
 * @param pwsc the pwsc we are emitting to
 * @param emit_header whether or not to throw out html headers and xml header
 * @returns XMLSTRUCT on success, or NULL if failure
 */
XMLSTRUCT *xml_init(WS_CONNINFO *pwsc, int emit_header) {
    XMLSTRUCT *pxml;

    pxml=(XMLSTRUCT*)malloc(sizeof(XMLSTRUCT));
    if(!pxml) {
        DPRINTF(E_FATAL,L_XML,"Malloc error\n");
    }

    memset(pxml,0,sizeof(XMLSTRUCT));

    pxml->pwsc = pwsc;

    /* the world would be a wonderful place without ie */
    ws_addresponseheader(pwsc,"Cache-Control","no-cache");
    ws_addresponseheader(pwsc,"Expires","-1");

    if(emit_header) {
        ws_addresponseheader(pwsc,"Content-Type","text/xml; charset=utf-8");
        ws_writefd(pwsc,"HTTP/1.0 200 OK\r\n");
        ws_emitheaders(pwsc);

        ws_writefd(pwsc,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
    }

    return pxml;
}

/**
 * bulk set config
 */
void xml_update_config(WS_CONNINFO *pwsc) {
    char *arg, *value, *duparg;
    char *ptmp;
    char *badparms=NULL;
    void *handle;
    int has_error=0;
    int badparms_len=0;
    int err;

    handle = NULL;


    while((handle=ws_enum_var(pwsc,&arg,&value,handle)) != NULL) {
        /* arg will be section:value */
        duparg = strdup(arg);
        ptmp = strchr(duparg,':');
        if(ptmp) {
            *ptmp++ = '\0';
            /* this is stupidly inefficient */

            DPRINTF(E_DBG,L_XML,"Setting %s/%s to %s\n",duparg,ptmp,value);
            err = conf_set_string(duparg,ptmp,value,TRUE);
            if(err != CONF_E_SUCCESS) {
                DPRINTF(E_DBG,L_XML,"Error setting %s/%s\n",duparg,ptmp);
                has_error = TRUE;
                if(!badparms) {
                    badparms_len = (int)strlen(arg) + 1;
                    badparms = (char*)malloc(badparms_len);
                    if(!badparms) {
                        DPRINTF(E_FATAL,L_XML,"xml_update_config: malloc\n");
                    }
                    strcpy(badparms,arg);
                } else {
                    badparms_len += (int)strlen(arg) + 1;
                    badparms = (char*)realloc(badparms,badparms_len);
                    if(!badparms) {
                        DPRINTF(E_FATAL,L_XML,"xml_update_config: malloc\n");
                    }
                    strcat(badparms,",");
                    strcat(badparms,arg);
                }
            }
        }
        free(duparg);
    }

    if(has_error) {
        DPRINTF(E_INF,L_XML,"Bad parms; %s\n",badparms);
        xml_return_error(pwsc,500,badparms);
        free(badparms);
        return;
    }

    handle = NULL;

    /* now set! */
    while((handle=ws_enum_var(pwsc,&arg,&value,handle)) != NULL) {
        /* arg will be section:value */
        duparg = strdup(arg);
        ptmp = strchr(duparg,':');
        if(ptmp) {
            *ptmp++ = '\0';
            /* this is stupidly inefficient */

            DPRINTF(E_DBG,L_XML,"Setting %s/%s to %s\n",duparg,ptmp,value);
            err = conf_set_string(duparg,ptmp,value,FALSE);
            if(err != CONF_E_SUCCESS) {
                /* shouldn't happen */
                DPRINTF(E_DBG,L_XML,"Error setting %s/%s\n",duparg,ptmp);
                xml_return_error(pwsc,500,arg);
                return;
            }
        }
        free(duparg);
    }

    xml_return_error(pwsc,200,"Success");
}

/**
 * rescan the database
 */
void xml_rescan(WS_CONNINFO *pwsc) {
    if(ws_getvar(pwsc,"full")) {
        config.full_reload=1;
    }

    config.reload=1;

    xml_return_error(pwsc,200,"Success");
}

/**
 * post settings back to the config file
 *
 * @param pwsc connection do dump results back to
 */
void xml_set_config(WS_CONNINFO *pwsc) {
    char *section;
    char *key;
    char *value;
    int verify_only;
    int err;

    section = ws_getvar(pwsc,"section");
    key = ws_getvar(pwsc,"key");
    value = ws_getvar(pwsc,"value");

    verify_only=0;
    if(ws_getvar(pwsc,"verify_only")) {
        verify_only = 1;
    }

    if((!section) || (!key) || (!value)) {
        xml_return_error(pwsc,500,"Missing section, key, or value");
        return;
    }

    if((err=conf_set_string(section,key,value,verify_only)!=CONF_E_SUCCESS)) {
        /* should return text error from conf_ */
        switch(err) {
        case CONF_E_BADELEMENT:
            xml_return_error(pwsc,500,"Unknown section/key pair");
            break;
        case CONF_E_PATHEXPECTED:
            xml_return_error(pwsc,500,"Expecting valid path");
            break;
        case CONF_E_INTEXPECTED:
            xml_return_error(pwsc,500,"Expecting integer value");
            break;
        case CONF_E_NOTWRITABLE:
            xml_return_error(pwsc,500,"Config file not writable");
            break;
        default:
            xml_return_error(pwsc,500,"conf_set_string: error");
        }
        return;
    }

    xml_return_error(pwsc,200,"Success");
    return;
}

/**
 * push a new term on the stack
 *
 * @param pxml xml struct obtained from xml_init
 * @param term next xlm section to start
 */
void xml_push(XMLSTRUCT *pxml, char *term) {
    XMLSTACK *pstack;

    pstack = (XMLSTACK *)malloc(sizeof(XMLSTACK));
    pstack->next=pxml->stack.next;
    pstack->tag=strdup(term);
    pxml->stack.next=pstack;

    pxml->stack_level++;

    ws_writefd(pxml->pwsc,"<%s>",term);
}

/**
 * end an xml section
 *
 * @param pxml xml struct we are working with
 */
void xml_pop(XMLSTRUCT *pxml) {
    XMLSTACK *pstack;

    pstack=pxml->stack.next;
    if(!pstack) {
        DPRINTF(E_LOG,L_XML,"xml_pop: tried to pop an empty stack\n");
        return;
    }

    pxml->stack.next = pstack->next;

    ws_writefd(pxml->pwsc,"</%s>",pstack->tag);
    free(pstack->tag);
    free(pstack);

    pxml->stack_level--;
}

/* FIXME: Fixed at 256?  And can't I get an expandable sprintf/cat? */

/**
 * output a string
 */
void xml_output(XMLSTRUCT *pxml, char *section, char *fmt, ...) {
    va_list ap;
    char buf[256];
    char *output;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    output = xml_entity_encode(buf);
    if(section) {
        xml_push(pxml,section);
    }
    ws_writefd(pxml->pwsc,"%s",output);
    free(output);
    if(section) {
        xml_pop(pxml);
    }
}

/**
 * clean up an xml struct
 *
 * @param pxml xml struct to clean up
 */
void xml_deinit(XMLSTRUCT *pxml) {
    XMLSTACK *pstack;

    if(pxml->stack.next) {
        DPRINTF(E_LOG,L_XML,"xml_deinit: entries still on stack (%s)\n",
                pxml->stack.next->tag);
    }

    while((pstack=pxml->stack.next)) {
        pxml->stack.next=pstack->next;
        free(pstack->tag);
        free(pstack);
    }
    free(pxml);
}

/**
 * main entrypoint for the xmlrpc functions.
 *
 * @arg pwsc Pointer to the web request structure
 */
void xml_handle(WS_CONNINFO *pwsc) {
    char *method;

    if((method=ws_getvar(pwsc,"method")) == NULL) {
        ws_returnerror(pwsc,500,"no method specified");
        return;
    }

    if(strcasecmp(method,"stats") == 0) {
        xml_get_stats(pwsc);
        return;
    }

    if(strcasecmp(method,"config") == 0) {
        conf_xml_dump(pwsc);
        return;
    }

    if(strcasecmp(method,"setconfig") == 0) {
        xml_set_config(pwsc);
        return;
    }

    if(strcasecmp(method,"updateconfig") == 0) {
        xml_update_config(pwsc);
        return;
    }

#if(0)  /* Turn this off until it's done in the web admin */
    if(strcasecmp(method,"browse_path") == 0) {
        xml_browse_path(pwsc);
        return;
    }
#endif

    if(strcasecmp(method,"rescan") == 0) {
        xml_rescan(pwsc);
        return;
    }

    if(strcasecmp(method,"shutdown") == 0) {
        config.stop=1;
        xml_return_error(pwsc,200,"Success");
        return;
    }

    xml_return_error(pwsc,500,"Invalid method");
    return;
}

/**
 * Given a base directory, walk through the directory and enumerate
 * all folders underneath it.
 *
 * @param pwsc connection over which to dump the result
 */
void xml_browse_path(WS_CONNINFO *pwsc) {
    XMLSTRUCT *pxml;
    DIR *pd;
    char *base_path;
    char de[sizeof(struct dirent) + MAXNAMLEN + 1];
    struct dirent *pde;
    int readable, writable;
    char full_path[PATH_MAX+1];
    char resolved_path[PATH_MAX];
    int err;
    int ignore_dotfiles=1;
    int ignore_files=1;
    struct stat sb;

    base_path = ws_getvar(pwsc, "path");
    if(!base_path)
        base_path = PATHSEP_STR;

    if(ws_getvar(pwsc,"show_dotfiles"))
        ignore_dotfiles = 0;

    if(ws_getvar(pwsc,"show_files"))
        ignore_files = 0;

    pd = opendir(base_path);
    if(!pd) {
        xml_return_error(pwsc,500,"Bad path");
        return;
    }

    pxml=xml_init(pwsc,1);
    xml_push(pxml,"results");

    /* get rid of trailing slash */
    while(1) {
        pde = (struct dirent *)&de;
        err = readdir_r(pd,(struct dirent *)de, &pde);

        if(err == -1) {
            DPRINTF(E_LOG,L_SCAN,"Error in readdir_r: %s\n",
                    strerror(errno));
            break;
        }

        if(!pde)
            break;

        if((!strcmp(pde->d_name,".")) || (!strcmp(pde->d_name,"..")))
            continue;


        snprintf(full_path,PATH_MAX,"%s%c%s",base_path,PATHSEP,pde->d_name);
        realpath(full_path,resolved_path);

        if(os_stat(resolved_path,&sb)) {
            DPRINTF(E_INF,L_XML,"Error statting %s: %s\n",
                    resolved_path,strerror(errno));
            continue;
        }

        if((sb.st_mode & S_IFREG) && (ignore_files))
            continue;

        /* skip symlinks and devices and whatnot */
        if((!(sb.st_mode & S_IFDIR)) &&
           (!(sb.st_mode & S_IFREG)))
            continue;

        if((ignore_dotfiles) && (pde->d_name) && (pde->d_name[0] == '.'))
            continue;

        readable = !access(resolved_path,R_OK);
        writable = !access(resolved_path,W_OK);

        if(sb.st_mode & S_IFDIR) {
            xml_push(pxml,"directory");
        } else if((sb.st_mode & S_IFLNK) == S_IFLNK) {
            xml_push(pxml,"symlink");
        } else {
            xml_push(pxml,"file");
        }
        xml_output(pxml,"name",pde->d_name);
        xml_output(pxml,"full_path",resolved_path);
        xml_output(pxml,"readable","%d",readable);
        xml_output(pxml,"writable","%d",writable);

        xml_pop(pxml); /* directory */

    }


    xml_pop(pxml);
    xml_deinit(pxml);

    closedir(pd);
}

/**
 * return xml file of all playlists
 */
void xml_get_stats(WS_CONNINFO *pwsc) {
    int r_secs, r_days, r_hours, r_mins;
    char buf[80];
    WS_CONNINFO *pci;
    SCAN_STATUS *pss;
    WSTHREADENUM wste;
    int count;
    XMLSTRUCT *pxml;
    void *phandle;

    pxml=xml_init(pwsc,1);
    xml_push(pxml,"status");

    xml_push(pxml,"service_status");

    xml_push(pxml,"service");

    xml_output(pxml,"name","Rendezvous");

#ifndef WITHOUT_MDNS
    if(config.use_mdns) {
        xml_output(pxml,"status",rend_running() ? "Running" : "Stopped"); /* ??? */
    } else {
        xml_output(pxml,"status","Disabled");
    }
#else
    ws_writefd(pwsc,"<td>No Support</td><td>&nbsp;</td></tr>\n");
#endif
    xml_pop(pxml); /* service */

    xml_push(pxml,"service");
    xml_output(pxml,"name","DAAP Server");
    xml_output(pxml,"status",config.stop ? "Stopping" : "Running");
    xml_pop(pxml); /* service */

    xml_push(pxml,"service");
    xml_output(pxml,"name","File Scanner");
    xml_output(pxml,"status",config.reload ? "Running" : "Idle");
    xml_pop(pxml); /* service */

    xml_pop(pxml); /* service_status */

    xml_push(pxml,"plugins");
    phandle = NULL;
    while((phandle = plugin_enum(phandle))) {
        xml_push(pxml,"plugin");
        xml_output(pxml,"name",plugin_get_description(phandle));
        xml_pop(pxml); /* plugin */
    }

    xml_pop(pxml); /* plugins */

    xml_push(pxml,"thread_status");

    pci = ws_thread_enum_first(config.server,&wste);
    while(pci) {
        pss = ws_get_local_storage(pci);
        if(pss) {
            xml_push(pxml,"thread");
            xml_output(pxml,"id","%d",pss->thread);
            xml_output(pxml,"sourceip","%s",pss->host);
            xml_output(pxml,"action","%s",pss->what);
            xml_pop(pxml); /* thread */
        }
        pci=ws_thread_enum_next(config.server,&wste);
    }

    xml_pop(pxml); /* thread_status */

    xml_push(pxml,"statistics");

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

    xml_push(pxml,"stat");
    xml_output(pxml,"name","Uptime");
    xml_output(pxml,"value","%s",buf);
    xml_pop(pxml); /* stat */

    xml_push(pxml,"stat");
    xml_output(pxml,"name","Songs");
    db_get_song_count(NULL,&count);
    xml_output(pxml,"value","%d",count);
    xml_pop(pxml); /* stat */

    xml_push(pxml,"stat");
    xml_output(pxml,"name","Songs Served");
    xml_output(pxml,"value","%d",config.stats.songs_served);
    xml_pop(pxml); /* stat */

    xml_pop(pxml); /* statistics */


    xml_push(pxml,"misc");
    xml_output(pxml,"writable_config","%d",conf_iswritable());
    xml_output(pxml,"config_path","%s",conf_get_filename());
    xml_output(pxml,"version","%s",VERSION);
    xml_pop(pxml); /* misc */

    xml_pop(pxml); /* status */

    xml_deinit(pxml);
    return;
}

/**
 * xml entity encoding, stupid style
 */
char *xml_entity_encode(char *original) {
    char *new;
    char *s, *d;
    int destsize;

    destsize = 6*(int)strlen(original)+1;
    new=(char *)malloc(destsize);
    if(!new) return NULL;

    memset(new,0x00,destsize);

    s=original;
    d=new;

    while(*s) {
        switch(*s) {
        case '>':
            strcat(d,"&gt;");
            d += 4;
            s++;
            break;
        case '<':
            strcat(d,"&lt;");
            d += 4;
            s++;
            break;
        case '"':
            strcat(d,"&quot;");
            d += 6;
            s++;
            break;
        case '\'':
            strcat(d,"&apos;");
            d += 6;
            s++;
            break;
        case '&':
            strcat(d,"&amp;");
            d += 5;
            s++;
            break;
        default:
            *d++ = *s++;
        }
    }

    return new;
}
