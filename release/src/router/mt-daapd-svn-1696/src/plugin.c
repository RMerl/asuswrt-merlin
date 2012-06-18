/*
 * $Id: $
 * Simple plug-in api for output, transcode, and scanning plug-ins
 *
 * Copyright (C) 2006 Ron Pedde (ron@pedde.com)
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define _XOPEN_SOURCE 500  /** unix98?  pthread_once_t, etc */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "daapd.h"
#include "conf.h"
#include "configfile.h"
#include "db-generic.h"
#include "err.h"
#include "io.h"
#include "os.h"
#include "plugin.h"
#include "rend.h"
#include "restart.h"
#include "smart-parser.h"
#include "xml-rpc.h"
#include "webserver.h"
#include "ff-plugins.h"
#include "io.h"

typedef struct tag_pluginentry {
    void *phandle;
    PLUGIN_INFO *pinfo;
    struct tag_pluginentry *next;
} PLUGIN_ENTRY;

/* Globals */
static PLUGIN_ENTRY _plugin_list;
static int _plugin_initialized = 0;
static char *_plugin_ssc_codecs = NULL;

static char* _plugin_error_list[] = {
    "Success.",
    "Could not load plugin: %s",
    "Plugin missing required export: plugin_type/plugin_ver"
};

/* Forwards */
void _plugin_readlock(void);
void _plugin_writelock(void);
void _plugin_unlock(void);
int _plugin_error(char **pe, int error, ...);
void _plugin_free(int *pi);
void _plugin_recalc_codecs(void);
int _plugin_ssc_transcode(WS_CONNINFO *pwsc, MP3FILE *pmp3, int offset, int headers);

/**
 * initialize stuff for plugins
 *
 * @returns TRUE on success, FALSE otherwise
 */
int plugin_init(void) {
    return TRUE;
}

/**
 * free the tls
 */
void _plugin_free(int *pi) {
    if(pi)
        free(pi);
}

/**
 * deinitialize stuff for plugins
 *
 * @returns TRUE on success, FALSE otherwise
 */
int plugin_deinit(void) {
    return TRUE;
}

/**
 * return the error
 *
 * @param pe buffer to store the error string in
 * @param error error to return
 * @returns the specified error,to thelp with returns
 */
int _plugin_error(char **pe, int error, ...) {
    va_list ap;
    char errbuf[1024];

    if(!pe)
        return error;

    va_start(ap, error);
    vsnprintf(errbuf, sizeof(errbuf), _plugin_error_list[error], ap);
    va_end(ap);

    DPRINTF(E_SPAM,L_PLUG,"Raising error: %s\n",errbuf);

    *pe = strdup(errbuf);
    return error;
}

/**
 * walk through the installed plugins and recalculate
 * the codec string
 */
void _plugin_recalc_codecs(void) {
    PLUGIN_ENTRY *ppi;
    size_t size=0;

    ppi = _plugin_list.next;
    while(ppi) {
        if(ppi->pinfo->type & PLUGIN_TRANSCODE) {
            if(size) size++;
            size += strlen(ppi->pinfo->codeclist);
        }
        ppi=ppi->next;
    }

    if(_plugin_ssc_codecs) {
        free(_plugin_ssc_codecs);
    }

    _plugin_ssc_codecs = (char*)malloc(size+1);
    if(!_plugin_ssc_codecs) {
        DPRINTF(E_FATAL,L_PLUG,"_plugin_recalc_codecs: malloc\n");
    }

    memset(_plugin_ssc_codecs,0,size+1);

    ppi = _plugin_list.next;
    while(ppi) {
        if(ppi->pinfo->type & PLUGIN_TRANSCODE) {
            if(strlen(_plugin_ssc_codecs)) {
                strcat(_plugin_ssc_codecs,",");
            }
            strcat(_plugin_ssc_codecs,ppi->pinfo->codeclist);
        }
        ppi=ppi->next;
    }

    DPRINTF(E_DBG,L_PLUG,"New transcode codec list: %s\n",_plugin_ssc_codecs);
    return;

}

/**
 * plugin_get_description
 */
char *plugin_get_description(void *which) {
    PLUGIN_ENTRY *ppi = (PLUGIN_ENTRY *)which;

    return ppi->pinfo->server;
}


/**
 * walk through the loaded plugin list
 */
void *plugin_enum(void *where) {
    PLUGIN_ENTRY *ppi = (PLUGIN_ENTRY *)where;

    if(!ppi) {
        // _plugin_readlock();
        return (void*) _plugin_list.next;
    }

    if(!ppi->next) {
        // _plugin_unlock();
        return NULL;
    }

    return (void*) ppi->next;
}

/**
 * load a specified plugin.
 *
 * @param pe pointer to error string returned (if error)
 * @param plugin path to plugin to load
 *
 * return PLUGIN_E_SUCCESS, or not, with pe set
 */
int plugin_load(char **pe, char *path) {
    PLUGIN_ENTRY *ppi;
    void *phandle;
    PLUGIN_INFO *(*info_func)(void);
    PLUGIN_INFO *pinfo;

    DPRINTF(E_DBG,L_PLUG,"Attempting to load plugin %s\n",path);

    phandle = os_loadlib(pe, path);
    if(!phandle) {
        DPRINTF(E_INF,L_PLUG,"Couldn't get lib handle for %s\n",path);
        return PLUGIN_E_NOLOAD;
    }

    ppi = (PLUGIN_ENTRY*)malloc(sizeof(PLUGIN_ENTRY));
    memset(ppi,0x00,sizeof(PLUGIN_ENTRY));

    ppi->phandle = phandle;

    info_func = (PLUGIN_INFO*(*)(void)) os_libfunc(pe, phandle,"plugin_info");
    if(info_func == NULL) {
        DPRINTF(E_INF,L_PLUG,"Couldn't get info_func for %s\n",path);
        os_unload(phandle);
        free(ppi);
        return PLUGIN_E_BADFUNCS;
    }

    pinfo = info_func();
    ppi->pinfo = pinfo;

    if(!pinfo) {
        if(pe) *pe = strdup("plugin declined to load");
        os_unload(phandle);
        free(ppi);
        return PLUGIN_E_NOLOAD;
    }

    if(pinfo->version != PLUGIN_VERSION) {
        DPRINTF(E_INF,L_PLUG,"Plugin is too old: version %d, expecting %d\n",
                pinfo->version, PLUGIN_VERSION);
        os_unload(phandle);
        free(ppi);
        return PLUGIN_E_NOLOAD;
    }

    DPRINTF(E_INF,L_PLUG,"Loaded plugin %s (%s)\n",path,pinfo->server);

    if(!_plugin_initialized) {
        _plugin_initialized = 1;
        memset((void*)&_plugin_list,0,sizeof(_plugin_list));
    }

    ppi->next = _plugin_list.next;
    _plugin_list.next = ppi;

    _plugin_recalc_codecs();
    return PLUGIN_E_SUCCESS;
}

/**
 * check to see if we want to dispatch a particular url
 *
 * @param pwsc the connection info (including uri) to check
 * @returns TRUE if we want to handle it
 */
int plugin_url_candispatch(WS_CONNINFO *pwsc) {
    PLUGIN_ENTRY *ppi;

    ppi = _plugin_list.next;
    while(ppi) {
        if(ppi->pinfo->type & PLUGIN_OUTPUT) {
            if((ppi->pinfo->output_fns)->can_handle(pwsc)) {
                return TRUE;
            }
        }
        ppi = ppi->next;
    }

    return FALSE;
}

/**
 * actually DISPATCH the hander we said we wanted
 *
 * @param pwsc the connection info (including uri) to check
 * @returns TRUE if we want to handle it
 */
void plugin_url_handle(WS_CONNINFO *pwsc) {
    PLUGIN_ENTRY *ppi;
    void (*disp_fn)(WS_CONNINFO *pwsc);

    ppi = _plugin_list.next;
    while(ppi) {
        if(ppi->pinfo->type & PLUGIN_OUTPUT) {
            if((ppi->pinfo->output_fns)->can_handle(pwsc)) {
                /* we have a winner */
                DPRINTF(E_DBG,L_PLUG,"Dispatching %s to %s\n", ws_uri(pwsc),
                        ppi->pinfo->server);

                /* so functions must be a tag_plugin_output_fn */
                disp_fn=(ppi->pinfo->output_fns)->handler;
                disp_fn(pwsc);
                return;
            }
        }
        ppi = ppi->next;
    }

    /* should 500 here or something */
    ws_returnerror(pwsc, 500, "Can't find plugin handler");
    return;
}

/**
 * walk through the plugins and register whatever rendezvous
 * names the clients want
 */
int plugin_rend_register(char *name, int port, char *iface, char *txt) {
    PLUGIN_ENTRY *ppi;
    PLUGIN_REND_INFO *pri;
    char *supplied_txt;

    ppi = _plugin_list.next;

    while(ppi) {
        DPRINTF(E_DBG,L_PLUG,"Checking %s\n",ppi->pinfo->server);
        if(ppi->pinfo->rend_info) {
            pri = ppi->pinfo->rend_info;
            while(pri->type) {
                supplied_txt = pri->txt;
                if(!pri->txt)
                    supplied_txt = txt;

                DPRINTF(E_DBG,L_PLUG,"Registering %s\n",pri->type);
                rend_register(name,pri->type,port,iface,supplied_txt);
                pri++;
            }
        }
        ppi=ppi->next;
    }

    return TRUE;
}

/**
 * Test password for the handled namespace
 *
 * @param pwsc the connection info (including uri) to check
 * @param username user attempting to login
 * @param pw password attempting
 * @returns TRUE if we want to handle it
 */
int plugin_auth_handle(WS_CONNINFO *pwsc, char *username, char *pw) {
    PLUGIN_ENTRY *ppi;
    int (*auth_fn)(WS_CONNINFO *pwsc, char *username, char *pw);
    int result;

    ppi = _plugin_list.next;
    while(ppi) {
        if(ppi->pinfo->type & PLUGIN_OUTPUT) {
            if((ppi->pinfo->output_fns)->can_handle(pwsc)) {
                /* we have a winner */
                DPRINTF(E_DBG,L_PLUG,"Dispatching %s to %s\n", ws_uri(pwsc),
                        ppi->pinfo->server);

                /* so functions must be a tag_plugin_output_fn */
                auth_fn=(ppi->pinfo->output_fns)->auth;
                if(auth_fn) {
                    result=auth_fn(pwsc,username,pw);
                    return result;
                } else {
                    return TRUE;
                }
            }
        }
        ppi = ppi->next;
    }

    /* should 500 here or something */
    ws_returnerror(pwsc, 500, "Can't find plugin handler");
    return FALSE;
}

/**
 * send an event to a plugin... this can be a connection, disconnection, etc.
 */
void plugin_event_dispatch(int event_id, int intval, void *vp, int len) {
    PLUGIN_ENTRY *ppi;

    ppi = _plugin_list.next;
    while(ppi) {
        if(ppi->pinfo->type & PLUGIN_EVENT) {
/*            DPRINTF(E_DBG,L_PLUG,"Dispatching event %d to %s\n",
                event_id,ppi->versionstring); */

            if((ppi->pinfo->event_fns) && (ppi->pinfo->event_fns->handler)) {
                ppi->pinfo->event_fns->handler(event_id, intval, vp, len);
            }
        }
        ppi=ppi->next;
    }
}


/**
 * stupid helper to copy transcode stream to the fd
 */
int __plugin_ssc_copy(WS_CONNINFO *pwsc, PLUGIN_TRANSCODE_FN *pfn,
                     void *vp,int offset) {
    int bytes_read;
    int bytes_to_read;
    int total_bytes_read = 0;
    char buffer[1024];

    /* first, skip past the offset */
    while(offset) {
        bytes_to_read = sizeof(buffer);
        if(bytes_to_read > offset)
            bytes_to_read = offset;

        bytes_read = pfn->ssc_read(vp,buffer,bytes_to_read);
        if(bytes_read <= 0)
            return bytes_read;

        offset -= bytes_read;
    }

    while((bytes_read=pfn->ssc_read(vp,buffer,sizeof(buffer))) > 0) {
        total_bytes_read += bytes_read;
        if(ws_writebinary(pwsc,buffer,bytes_read) != bytes_read) {
            return total_bytes_read;
        }
    }

    /*
    if(bytes_read < 0) {
        return bytes_read;
    }
    */

    return total_bytes_read;
}

/**
 * do the transcode, emitting the headers, content type,
 * and shoving the file down the wire
 *
 * @param pwsc connection to transcode to
 * @param file file to transcode
 * @param codec source codec
 * @param duration time in ms
 * @returns bytes transferred, or -1 on error
 */
int plugin_ssc_transcode(WS_CONNINFO *pwsc, MP3FILE *pmp3, int offset, int headers) {
    PLUGIN_ENTRY *ppi, *ptc=NULL;
    PLUGIN_TRANSCODE_FN *pfn = NULL;
    void *vp_ssc;
    int post_error = 1;
    int result = -1;

    /* first, find the plugin that will do the conversion */
    ppi = _plugin_list.next;
    while((ppi) && (!pfn)) {
        if(ppi->pinfo->type & PLUGIN_TRANSCODE) {
            if(strstr(ppi->pinfo->codeclist,pmp3->codectype)) {
                ptc = ppi;
                pfn = ppi->pinfo->transcode_fns;
            }
        }
        ppi = ppi->next;
    }

    if(pfn) {
        DPRINTF(E_DBG,L_PLUG,"Transcoding %s with %s\n",pmp3->path,
                ptc->pinfo->server);

        vp_ssc = pfn->ssc_init();
        if(vp_ssc) {
            if(pfn->ssc_open(vp_ssc,pmp3)) {
                /* start reading and throwing */
                if(headers) {
                    ws_addresponseheader(pwsc,"Content-Type","audio/wav");
                    ws_addresponseheader(pwsc,"Connection","Close");
                    if(!offset) {
                        ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
                    } else {
                        ws_addresponseheader(pwsc,"Content-Range",
                                             "bytes %ld-*/*",
                                             (long)offset);
                        ws_writefd(pwsc,"HTTP/1.1 206 Partial Content\r\n");
                    }
                    ws_emitheaders(pwsc);
                }

                /* start reading/writing */
                result = __plugin_ssc_copy(pwsc,pfn,vp_ssc,offset);
                post_error = 0;
                pfn->ssc_close(vp_ssc);
            } else {
                DPRINTF(E_LOG,L_PLUG,"Error opening %s for ssc: %s\n",
                        pmp3->path,pfn->ssc_error(vp_ssc));
            }
            pfn->ssc_deinit(vp_ssc);
        } else {
            DPRINTF(E_LOG,L_PLUG,"Error initializing transcoder: %s\n",
                    ptc->pinfo->server);
        }
    }

    if(post_error) {
        pwsc->error = EPERM; /* ?? */
        ws_returnerror(pwsc,500,"Internal error");
    }

    return result;
}

int plugin_ssc_should_transcode(WS_CONNINFO *pwsc, char *codec) {
    int result;
    char *native_codecs=NULL;
    char *user_agent=NULL;
    char *never_transcode = NULL;
    char *always_transcode = NULL;

    ASSERT((pwsc) && (codec));

    if(!pwsc)
        return FALSE;

    if(!codec) {
        DPRINTF(E_LOG,L_PLUG,"testing transcode on null codec?\n");
        return FALSE;
    }

    never_transcode = conf_alloc_string("general","never_transcode",NULL);
    if(never_transcode) {
        if(strstr(never_transcode,codec)) {
            free(never_transcode);
            return FALSE;
        }
        free(never_transcode);
    }

    if(pwsc) {
        /* see if the headers give us any guidance */
        native_codecs = ws_getrequestheader(pwsc,"accept-codecs");
        if(!native_codecs) {
            user_agent = ws_getrequestheader(pwsc,"user-agent");
            if(user_agent) {
                if(strncmp(user_agent,"iTunes",6)==0) {
                    native_codecs = "mpeg,mp4a,wav,mp4v,alac";
                } else if(strncmp(user_agent,"Roku",4)==0) {
                    native_codecs = "mpeg,mp4a,wav,wma";
                } else if(strncmp(user_agent,"Hifidelio",9)==0) {
                    return FALSE;
                }
            }
        }
    }

    if(!native_codecs) {
        native_codecs = "mpeg,wav";
    }

    /* can't transcode it if we can't transcode it */
    if(!_plugin_ssc_codecs)
        return FALSE;

    always_transcode = conf_alloc_string("general","always_transcode",NULL);
    if(always_transcode) {
        if(strstr(always_transcode,codec)) {
            free(always_transcode);
            return TRUE;
        }
        free(always_transcode);
    }

    if(strstr(native_codecs,codec))
        return FALSE;

    result = FALSE;
    if(strstr(_plugin_ssc_codecs,codec)) {
        result = TRUE;
    }
    return result;
}

