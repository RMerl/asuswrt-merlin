/*
 * $Id: $
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ff-dbstruct.h"
#include "ff-plugins.h"

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif

/* Forwards */
void *ssc_script_init(void);
void ssc_script_deinit(void *vp);
int ssc_script_open(void *vp, MP3FILE *pmp3);
int ssc_script_close(void *vp);
int ssc_script_read(void *vp, char *buffer, int len);
char *ssc_script_error(void *vp);

PLUGIN_INFO *plugin_info(void);

#define infn ((PLUGIN_INPUT_FN *)(_pi.pi))

/* Globals */
PLUGIN_TRANSCODE_FN _ptfn = {
    ssc_script_init,
    ssc_script_deinit,
    ssc_script_open,
    ssc_script_close,
    ssc_script_read,
    ssc_script_error
};

PLUGIN_INFO _pi = {
    PLUGIN_VERSION,        /* version */
    PLUGIN_TRANSCODE,      /* type */
    "ssc-script/" VERSION, /* server */
    NULL,                  /* output fns */
    NULL,                  /* event fns */
    &_ptfn,                /* fns */
    NULL,                  /* rend info */
    NULL                   /* codeclist */
};

typedef struct tag_ssc_handle {
    FILE *fin;
} SSCHANDLE;

static char *_ssc_script_program = NULL;

/**
 * return the plugininfo struct to firefly
 */
PLUGIN_INFO *plugin_info(void) {
    char *codeclist;

    _ssc_script_program = pi_conf_alloc_string("general","ssc_prog",NULL);
    if(!_ssc_script_program) {
        pi_log(E_INF,"No ssc program specified for script transcoder.\n");
        return NULL;
    }

    /* FIXME: need an unload function to stop leak */
    codeclist = pi_conf_alloc_string("general","ssc_codectypes",NULL);
    if(!codeclist) {
        pi_log(E_INF,"No codectypes specified for script transcoder.\n");
        return NULL;
    }

    _pi.codeclist = codeclist;
    return &_pi;
}


/*
 * get a new transcode handle
 */
void *ssc_script_init(void) {
    SSCHANDLE *handle;

    handle = (SSCHANDLE*)malloc(sizeof(SSCHANDLE));
    if(handle) {
        memset(handle,0,sizeof(SSCHANDLE));
    }

    return (void*)handle;
}

/**
 * FIXME: make register errors in the sschandle
 */
char *ssc_script_error(void *vp) {
    return "Unknown";
}


/**
 * dispose of the transocde handle obtained from init
 *
 * @param pv handle to dispose
 */
void ssc_script_deinit(void *vp) {
    SSCHANDLE *handle = (SSCHANDLE*)vp;

    if(handle->fin) {
        pclose(handle->fin);
    }

    if(handle)
        free(handle);
}

/**
 * open a file to transocde
 *
 * @param pv private sschandle obtained from init
 * @param file file name to transcode
 * @param codec codec type
 * @param duration duration in ms
 */
int ssc_script_open(void *vp, MP3FILE *pmp3) {
    SSCHANDLE *handle = (SSCHANDLE*)vp;
    char *cmd;
    char *newpath;
    char *metachars = "\"\\!(){}#*?$&<>`"; /* More?? */
    char metacount = 0;
    char *src,*dst;
    char *file;
    char *codec;
    int duration;

    file = pmp3->path;
    codec = pmp3->codectype;
    duration = pmp3->song_length;


    src=file;
    while(*src) {
        if(strchr(metachars,*src))
            metacount+=5;
        src++;
    }

    if(metachars) {
        newpath = (char*)malloc(strlen(file) + metacount + 1);
        if(!newpath) {
            pi_log(E_FATAL,"ssc_script_open: malloc\n");
        }
        src=file;
        dst=newpath;

        while(*src) {
            if(strchr(metachars,*src)) {
                *dst++='"';
                *dst++='\'';
                *dst++=*src++;
                *dst++='\'';
                *dst++='"';
            } else {
                *dst++=*src++;
            }
        }
        *dst='\0';
    } else {
        newpath = strdup(file); /* becuase it will be freed... */
    }

    /* FIXME: is 64 enough? is there a better way to determine this? */
    cmd=(char *)malloc(strlen(_ssc_script_program) +
                       strlen(file) +
                       64);
    sprintf(cmd, "%s \"%s\" 0 %lu.%03lu \"%s\"",
            _ssc_script_program, newpath, (unsigned long) duration / 1000,
            (unsigned long)duration % 1000, (codec && *codec) ? codec : "*");
    pi_log(E_INF,"Executing %s\n",cmd);
    handle->fin = popen(cmd, "r");
    free(newpath);
    free(cmd);  /* should really have in-place expanded the path */

    return TRUE;
}

int ssc_script_close(void *vp) {
    SSCHANDLE *handle = (SSCHANDLE*)vp;

    if(handle->fin) {
        pclose(handle->fin);
        handle->fin=NULL;
    }

    return TRUE;
}

int ssc_script_read(void *vp, char *buffer, int len) {
    SSCHANDLE *handle = (SSCHANDLE*)vp;

    return fread(buffer,1,len,handle->fin);
}

