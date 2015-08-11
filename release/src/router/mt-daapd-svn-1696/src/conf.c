/*
 * $Id: conf.c 1690 2007-10-23 04:23:50Z rpedde $
 * Functions for reading and writing the config file
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


/**
 * \file config.c
 *
 * Config file reading and writing
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#include "daapd.h"
#include "conf.h"
#include "err.h"
#include "ll.h"
#include "os.h"
#include "util.h"
#include "webserver.h"
#include "xml-rpc.h"

#include "io.h"

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

#define MAX_REND_LEN 62

/** Globals */
//static int ecode;
static LL_HANDLE conf_main=NULL;
static LL_HANDLE conf_comments=NULL;

static char *conf_main_file = NULL;

#define CONF_LINEBUFFER 1024

#define CONF_T_INT          0
#define CONF_T_STRING       1
#define CONF_T_EXISTPATH    2  /** a path that must exist */
#define CONF_T_MULTICOMMA   3  /** multiple entries separated by commas */
#define CONF_T_MULTIPATH    4  /** multiple comma separated paths */

typedef struct _CONF_ELEMENTS {
    int required;
    int deprecated;
    int type;
    char *section;
    char *term;
} CONF_ELEMENTS;

typedef struct _CONF_MAP {
    char *old_section;
    char *old_key;
    char *new_section;
    char *new_key;
} CONF_MAP;

/** Forwards */
static int _conf_verify(LL_HANDLE pll);
static LL_ITEM *_conf_fetch_item(LL_HANDLE pll, char *section, char *key);
static int _conf_exists(LL_HANDLE pll, char *section, char *key);
static int _conf_write(IOHANDLE hfile, LL *pll, int sublevel, char *parent);
static CONF_ELEMENTS *_conf_get_keyinfo(char *section, char *key);
static int _conf_makedir(char *path, char *user);
static int _conf_existdir(char *path);
static int _conf_xml_dump(XMLSTRUCT *pxml,LL *pll,int sublevel,char *parent);
static int _conf_verify_element(char *section, char *key, char *value);
static void _conf_remap_entry(char *old_section, char *old_key, char **new_section, char **new_key);

static CONF_ELEMENTS conf_elements[] = {
    { 1, 0, CONF_T_STRING,"general","runas" },
    { 1, 0, CONF_T_EXISTPATH,"general","web_root" },
    { 0, 0, CONF_T_INT,"general","port" },
    { 0, 0, CONF_T_STRING,"general","admin_pw" },
    { 1, 0, CONF_T_MULTIPATH,"general","mp3_dir" },
    { 0, 1, CONF_T_EXISTPATH,"general","db_dir" },
    { 0, 0, CONF_T_STRING,"general","db_type" },
    { 0, 0, CONF_T_EXISTPATH,"general","db_parms" }, /* this isn't right */
    { 0, 0, CONF_T_INT,"general","debuglevel" },
    { 0, 0, CONF_T_STRING,"general","servername" },
    { 0, 0, CONF_T_INT,"general","rescan_interval" },
    { 0, 0, CONF_T_INT,"general","always_scan" },
    { 0, 1, CONF_T_INT,"general","latin1_tags" },
    { 0, 0, CONF_T_INT,"general","process_m3u" },
    { 0, 0, CONF_T_INT,"general","scan_type" },
    { 0, 1, CONF_T_INT,"general","compress" },
    { 0, 1, CONF_T_STRING,"general","playlist" },
    { 0, 0, CONF_T_STRING,"general","extensions" },
    { 0, 0, CONF_T_STRING,"general","interface" },
    { 0, 0, CONF_T_STRING,"general","ssc_codectypes" },
    { 0, 0, CONF_T_STRING,"general","ssc_prog" },
    { 0, 0, CONF_T_STRING,"general","password" },
    { 0, 0, CONF_T_STRING,"general","never_transcode" },
    { 0, 0, CONF_T_MULTICOMMA,"general","compdirs" },
    { 0, 0, CONF_T_STRING,"general","logfile" },
    { 0, 0, CONF_T_INT,"general","truncate" },
    { 0, 0, CONF_T_EXISTPATH,"plugins","plugin_dir" },
    { 0, 0, CONF_T_MULTICOMMA,"plugins","plugins" },
    { 0, 0, CONF_T_INT,"daap","empty_strings" },
    { 0, 0, CONF_T_INT,"daap","supports_browse" },
    { 0, 0, CONF_T_INT,"daap","supports_update" },
    { 0, 0, CONF_T_INT,"scanning","process_xml" },
    { 0, 0, CONF_T_INT,"scanning","ignore_appledouble" },
    { 0, 0, CONF_T_INT,"scanning","ignore_dotfiles" },
    { 0, 0, CONF_T_INT,"scanning","concat_compilations" },
    { 0, 0, CONF_T_INT,"scanning","case_sensitive" },
    { 0, 0, CONF_T_INT,"scanning","follow_symlinks" },
    { 0, 0, CONF_T_INT,"scanning","skip_first" },
    { 0, 0, CONF_T_STRING,"scanning","mp3_tag_codepage" },
    { 0, 0, CONF_T_INT,"scan","correct_order" },

    /* remapped values */
    { 0, 0, CONF_T_INT,"scanning","process_playlists" },
    { 0, 0, CONF_T_INT,"scanning","process_itunes" },
    { 0, 0, CONF_T_INT,"scanning","process_m3u" },
    { 0, 0, CONF_T_INT,"daap","correct_order" },
    { 0, 0, CONF_T_INT, NULL, NULL },
};

static CONF_MAP _conf_map[] = {
    { "general","process_m3u","scanning","process_playlists" },
    { "scanning","process_xml","scanning","process_itunes" },
    { "scan","correct_order","daap","correct_order" },

    { NULL, NULL, NULL, NULL }
};

/**
 * Upgrade a config silently by remapping old configuration values
 * to new configuration values
 */
void _conf_remap_entry(char *old_section, char *old_key, char **new_section, char **new_key) {
    CONF_MAP *pmap;

    pmap = _conf_map;
    while(pmap->old_section) {
        if((!strcasecmp(old_section,pmap->old_section)) &&
           (!strcasecmp(old_key,pmap->old_key))) {
            /* found it! */
            DPRINTF(E_LOG,L_CONF,"Config option %s/%s has "
                    "been replaced by %s/%s\n",
                    pmap->old_section,pmap->old_key,
                    pmap->new_section, pmap->new_key);
            *new_section = pmap->new_section;
            *new_key = pmap->new_key;
            return;
        }
        pmap++;
    }

    *new_section = old_section;
    *new_key = old_key;
}

/**
 * Try and create a directory, including parents (probably
 * in response to a config file entry that does not exist).
 *
 * @param path path to make
 * @returns TRUE on success, FALSE otherwise
 */

int _conf_makedir(char *path,char *user) {
    char *token, *next_token;
    char *pathdup;
    char path_buffer[PATH_MAX];
    int retval = FALSE;

    DPRINTF(E_DBG,L_CONF,"Creating %s\n",path);

    pathdup=strdup(path);
    if(!pathdup) {
        DPRINTF(E_FATAL,L_CONF,"Malloc error\n");
    }

    next_token=pathdup;
    while(*next_token && (*next_token != PATHSEP))
        next_token++;
    if(*next_token)
        next_token++;

    memset(path_buffer,0,sizeof(path_buffer));

    while((token=strsep(&next_token,PATHSEP_STR))) {
        if((strlen(path_buffer) + strlen(token)) < PATH_MAX) {
            strcat(path_buffer,PATHSEP_STR);
            strcat(path_buffer,token);

            if(!_conf_existdir(path_buffer)) {
                DPRINTF(E_DBG,L_CONF,"Making %s\n",path_buffer);
                if((mkdir(path_buffer,0700)) && (errno != EEXIST)) {
                    free(pathdup);
                    DPRINTF(E_LOG,L_CONF,"Could not make dirctory %s: %s\n",
                            path_buffer,strerror(errno));
                    return FALSE;
                }
                os_chown(path_buffer,user);
            }
            retval = TRUE;
        }
    }

    free(pathdup);
    return retval;
}

/**
 * Determine if a particular directory exists or not
 *
 * @param path directory to test for existence
 * @returns true if path exists, false otherwise
 */
int _conf_existdir(char *path) {
    struct stat sb;

    DPRINTF(E_DBG,L_CONF,"Checking existence of %s\n",path);

    if(os_stat(path,&sb)) {
        return FALSE;
    }

    if(sb.st_mode & S_IFDIR)
        return TRUE;

    return FALSE;
}

/**
 * given a section and key, get the conf_element for it.
 * right now this is simple, but eventually there might
 * be more difficult mateches to be made
 *
 * @param section section the key was found ind
 * @param key key we are searching for info on
 * @returns the CONF_ELEMENT that is the closest match, or
 *          NULL if no match was found.
 */
CONF_ELEMENTS *_conf_get_keyinfo(char *section, char *key) {
    CONF_ELEMENTS *pcurrent;
    int found=0;

    pcurrent = &conf_elements[0];
    while(pcurrent->section && pcurrent->term) {
        if((strcasecmp(section,pcurrent->section) != 0) ||
           (strcasecmp(key,pcurrent->term) != 0)) {
            pcurrent++;
        } else {
            found = 1;
            break;
        }
    }

    return found ? pcurrent : NULL;
}

/**
 * fetch item based on section/term basis, rather than just a single
 * level deep, like ll_fetch_item does
 *
 * @param pll top level linked list to test (config tree)
 * @param section section to term (key) is in
 * @param key key to look for
 * @returns LL_ITEM of the key, or NULL
 */
LL_ITEM *_conf_fetch_item(LL_HANDLE pll, char *section, char *key) {
    LL_ITEM *psection;
    LL_ITEM *pitem;

    if(!pll)
        return NULL;

    if(!(psection = ll_fetch_item(pll,section)))
        return NULL;

    if(psection->type != LL_TYPE_LL)
        return NULL;

    if(!(pitem = ll_fetch_item(psection->value.as_ll,key)))
        return NULL;

    return pitem;
}

/**
 * simple test to see if a particular section/key value exists
 *
 * @param pll config tree to test
 * @param section section to find the term under
 * @param key key to search for under the specified section
 * @returns TRUE if key exists, FALSE otherwise
 */
int _conf_exists(LL_HANDLE pll, char *section, char *key) {
    if(!_conf_fetch_item(pll,section,key))
           return FALSE;

    return TRUE;
}


/**
 * verify a specific element, that is, see if changing a specific
 * element, in a vacuum, would be problematic
 *
 * @param section section to test
 * @param key key to test
 * @param value value to test
 * @returns CONF_E_SUCCESS on success
 */

int _conf_verify_element(char *section, char *key, char *value) {
    CONF_ELEMENTS *pce;
    int index;
    char **valuearray;

    pce = _conf_get_keyinfo(section, key);
    if(!pce) {
        return CONF_E_BADELEMENT;
    }

    if(strcmp(value,"") == 0) {
        if(!pce->required) {
            return CONF_E_SUCCESS;
        }
    }

    switch(pce->type) {
    case CONF_T_MULTICOMMA: /* can't really check these */
    case CONF_T_STRING:
        return CONF_E_SUCCESS;
        break;

    case CONF_T_INT:
        if((atoi(value) || (strcmp(value,"0")==0)))
           return CONF_E_SUCCESS;
        return CONF_E_INTEXPECTED;
        break;

    case CONF_T_MULTIPATH:
        if(util_split(value,",",&valuearray) >= 0) {
            index = 0;
            while(valuearray[index]) {
                if(!_conf_existdir(valuearray[index])) {
                    util_dispose_split(valuearray);
                    return CONF_E_PATHEXPECTED;
                }
                index++;
            }
            util_dispose_split(valuearray);
        }
        break;

    case CONF_T_EXISTPATH:
        if(!_conf_existdir(value))
            return CONF_E_PATHEXPECTED;
        return CONF_E_SUCCESS;
        break;

    default:
        DPRINTF(E_LOG,L_CONF,"Bad config type: %d\n",pce->type);
        break;

    }

    return CONF_E_SUCCESS;
}

/**
 * Verify that the configuration isn't obviously wrong.
 * Type checking has already been done, this just checks
 * required stuff isn't missing.
 *
 * @param pll tree to check
 * @returns TRUE if configuration appears valid, FALSE otherwise
 */
int _conf_verify(LL_HANDLE pll) {
    LL_ITEM *pi = NULL;
    LL_ITEM *ptemp = NULL;
    CONF_ELEMENTS *pce;
    int is_valid=TRUE;
    char resolved_path[PATH_MAX];
    char *user;

    /* first, walk through the elements and make sure
     * all required elements are there */
    pce = &conf_elements[0];
    while(pce->section) {
        if(pce->required) {
            if(!_conf_exists(pll,pce->section, pce->term)) {
                DPRINTF(E_LOG,L_CONF,"Missing configuration entry "
                    " %s/%s.  Please review the sample config\n",
                    pce->section, pce->term);
                is_valid=FALSE;
            }
        }
        if(pce->deprecated) {
            if(_conf_exists(pll,pce->section,pce->term)) {
                DPRINTF(E_LOG,L_CONF,"Config entry %s/%s is deprecated.  Please "
                        "review the sample config\n",
                        pce->section, pce->term);
            }
        }
        if(pce->type == CONF_T_EXISTPATH) {
            /* first, need to resolve */
            pi = _conf_fetch_item(pll,pce->section, pce->term);
            if(pi) {
                memset(resolved_path,0,sizeof(resolved_path));
                if(pi->value.as_string) {
                    DPRINTF(E_SPAM,L_CONF,"Found %s/%s as %s... checking\n",
                            pce->section, pce->term, pi->value.as_string);

                    /* verify it exists, creating it if necessary */
                    if(!_conf_existdir(pi->value.as_string)) {
                        user = "nobody";
                        ptemp = _conf_fetch_item(pll, "general", "runas");
                        if(ptemp) {
                            user = ptemp->value.as_string;
                        }

                        if(!_conf_makedir(pi->value.as_string,user)) {
                            is_valid=0;
                            DPRINTF(E_LOG,L_CONF,"Can't make path %s, invalid config.\n",
                                    resolved_path);
                        }
                    }

                    if(_conf_existdir(pi->value.as_string)) {
                        realpath(pi->value.as_string,resolved_path);
                        free(pi->value.as_string);
                        pi->value.as_string = strdup(resolved_path);

                        DPRINTF(E_SPAM,L_CONF,"Resolved to %s\n",resolved_path);
                    }
                }
            }
        }
        pce++;
    }

    /* here we would walk through derived sections, if there
     * were any */

    return is_valid;
}


/**
 * apply an element
 */
void _conf_apply_element(char *section, char *key, char *cold, char *cnew) {
    if((strcmp(section,"general")==0) && (strcmp(key,"logfile")==0)) {
        /* logfile changed */
        if((cnew) && strlen(cnew)) {
            err_setlogfile(cnew);
            err_setdest(err_getdest() | LOGDEST_LOGFILE);
        } else {
            err_setdest(err_getdest() & ~LOGDEST_LOGFILE);
        }
    }

    if((strcmp(section,"general")==0) && (strcmp(key,"debuglevel")==0)) {
        /* loglevel changed */
        err_setlevel(atoi(cnew));
    }

    if((strcmp(section,"general")==0) && (strcmp(key,"truncate")==0)) {
        err_settruncate(atoi(cnew));
    }
}

/**
 * apply a new config.  This should really be called locked
 * just prior to applying the config.
 *
 * @param pll the "about to be applied" config
 */
void _conf_apply(LL_HANDLE pll) {
    LL_ITEM *psection;
    LL_ITEM *pitem, *polditem;
    char *oldvalue;
    char *newvalue;

    /* get all new and changed items */
    psection = NULL;
    while((psection=ll_get_next(pll, psection))) {
        /* walk through sections */
        pitem = NULL;
        while((pitem=ll_get_next(psection->value.as_ll,pitem))) {
            if(pitem->type == LL_TYPE_STRING) {
                newvalue = pitem->value.as_string;
                polditem = _conf_fetch_item(conf_main,
                                            psection->key,
                                            pitem->key);
                oldvalue = NULL;
                if(polditem)
                    oldvalue = polditem->value.as_string;

                _conf_apply_element(psection->key, pitem->key,
                                    oldvalue, newvalue);
            }
        }
    }

    /* now, get deleted items */
    psection = NULL;
    while((psection=ll_get_next(conf_main, psection))) {
        /* walk through sections */
        pitem = NULL;
        while((pitem=ll_get_next(psection->value.as_ll,pitem))) {
            if(pitem->type == LL_TYPE_STRING) {
                oldvalue = pitem->value.as_string;
                if(!_conf_fetch_item(pll,psection->key, pitem->key)) {
                    _conf_apply_element(psection->key, pitem->key,
                                        oldvalue,NULL);
                }
            }
        }
    }

    /* special check for config file */
}


/**
 * reload the existing config file.
 *
 * @returns CONF_E_SUCCESS on success
 */
int conf_reload(void) {
    return conf_read(conf_main_file);
}

/**
 * read a configfile into a tree
 *
 * @param file file to read
 * @returns TRUE if successful, FALSE otherwise
 */
int conf_read(char *file) {
    int err;
    LL_HANDLE pllnew, plltemp, pllcurrent, pllcomment;
    LL_ITEM *pli;
    char linebuffer[CONF_LINEBUFFER+1];
    char keybuffer[256];
    char conf_file[PATH_MAX+1];
    char *comment, *term, *value, *delim;
    char *section_name=NULL;
    char *prev_comments=NULL;
    int total_comment_length=CONF_LINEBUFFER;
    int current_comment_length=0;
    int compat_mode=1;
    int warned_truncate=0;
    int line=0;
    int ws=0;
    CONF_ELEMENTS *pce;
    int key_type;
    char **valuearray;
    int index;
    char *temp;
    IOHANDLE hconfig;
    uint32_t len;

    char *replaced_section = NULL;  /* config key/value updates */
    char *replaced_key = NULL;      /* config key/value updates */

    if(conf_main_file) {
        conf_close();
        free(conf_main_file);
    }

    realpath(file,conf_file);
    conf_main_file = strdup(conf_file);

    hconfig = io_new();
    if(!hconfig)
        DPRINTF(E_FATAL,L_CONF,"Malloc eror in io_new()\n");

    DPRINTF(E_DBG,L_CONF,"Loading config file %s\n",conf_file);
    if(!io_open(hconfig,"file://%U?ascii=1",conf_file)) {
        DPRINTF(E_LOG,L_MISC,"Error opening config file: %s\n",io_errstr(hconfig));
        io_dispose(hconfig);
        return CONF_E_FOPEN;
    }
    DPRINTF(E_DBG,L_CONF,"Config file open\n");

    prev_comments = (char*)malloc(total_comment_length);
    if(!prev_comments)
        DPRINTF(E_FATAL,L_CONF,"Malloc error\n");
    prev_comments[0] = '\0';

    if((err=ll_create(&pllnew)) != LL_E_SUCCESS) {
        DPRINTF(E_LOG,L_CONF,"Error creating linked list: %d\n",err);
        io_close(hconfig);
        io_dispose(hconfig);

        return CONF_E_UNKNOWN;
    }

    ll_create(&pllcomment);  /* don't care if we lose comments */

    comment = NULL;
    pllcurrent = NULL;

    /* got what will be the root of the config tree, now start walking through
     * the input file, populating the tree
     */

    /* TODO: linebuffered IO support */
    len = sizeof(linebuffer);
    while(io_readline(hconfig,(unsigned char *)linebuffer,&len) && len) {
        line++;
        linebuffer[CONF_LINEBUFFER] = '\0';
        ws=0;

        comment=strchr(linebuffer,'#');
        if(comment) {
            *comment = '\0';
            comment++;
        }

        while(strlen(linebuffer) &&
              (strchr("\n\r ",linebuffer[strlen(linebuffer)-1])))
            linebuffer[strlen(linebuffer)-1] = '\0';

        if(linebuffer[0] == '[') {
            /* section header */
            compat_mode=0;
            term=&linebuffer[1];
            value = strchr(term,']');

            /* convert spaces to underscores in headings */
            temp = term;
            while(*temp) {
                if(*temp == ' ')
                    *temp = '_';
                temp++;
            }

            if(!value) {
                ll_destroy(pllnew);
                io_close(hconfig);
                io_dispose(hconfig);
                return CONF_E_BADHEADER;
            }
            *value = '\0';

            if((err = ll_create(&plltemp)) != LL_E_SUCCESS) {
                ll_destroy(pllnew);
                io_close(hconfig);
                io_dispose(hconfig);
                return CONF_E_UNKNOWN;
            }

            ll_add_ll(pllnew,term,plltemp);

            /* set current section and name */
            if(section_name)
                free(section_name);
            section_name = strdup(term);

            /* set precomments */
            if(prev_comments[0] != '\0') {
                /* we had some preceding comments */
                snprintf(keybuffer,sizeof(keybuffer),"pre_%s",section_name);
                ll_add_string(pllcomment,keybuffer,prev_comments);
                prev_comments[0] = '\0';
                current_comment_length=0;
            }
            if(comment) {
                /* we had some preceding comments */
                snprintf(keybuffer,sizeof(keybuffer),"in_%s",section_name);
                ll_add_string(pllcomment,keybuffer,comment);
                prev_comments[0] = '\0';
                current_comment_length=0;
                comment = NULL;
            }
        } else {
            /* k/v pair */
            term=&linebuffer[0];

            while((*term=='\t') || (*term==' '))
                term++;

            value=term;

            if(compat_mode) {
                delim="\t ";
            } else {
                delim="=";
            }

            strsep(&value,delim);
            if((value) && (term) && (strlen(term))) {
                while((strlen(term) && (strchr("\t ",term[strlen(term)-1]))))
                    term[strlen(term)-1] = '\0';
                while(strlen(value) && (strchr("\t ",*value)))
                    value++;
                while((strlen(value) && (strchr("\t ",value[strlen(value)-1]))))
                    value[strlen(value)-1] = '\0';

                /* convert spaces to underscores in key */
                temp = term;
                while(*temp) {
                    if(*temp == ' ')
                        *temp = '_';
                    temp++;
                }

                if(!section_name) {
                    /* in compat mode -- add a general section */
                    if((err=ll_create(&plltemp)) != LL_E_SUCCESS) {
                        DPRINTF(E_LOG,L_CONF,"Error creating list: %d\n",err);
                        ll_destroy(pllnew);
                        io_close(hconfig);
                        io_dispose(hconfig);
                        return CONF_E_UNKNOWN;
                    }
                    ll_add_ll(pllnew,"general",plltemp);

                    if(section_name)
                        free(section_name); /* shouldn't ahppen */
                    section_name = strdup("general");

                    /* no inline comments, just precomments */
                    if(prev_comments[0] != '\0') {
                        /* we had some preceding comments */
                        ll_add_string(pllcomment,"pre_general",prev_comments);
                        prev_comments[0] = '\0';
                        current_comment_length=0;
                    }

                }

                /* see what kind this is, and act accordingly */
                _conf_remap_entry(section_name, term, &replaced_section, &replaced_key);
                DPRINTF(E_DBG,L_CONF,"Got %s/%s, convert to %s/%s (%s)\n",section_name,
                        term, replaced_section, replaced_key, value);

                pli = ll_fetch_item(pllnew,replaced_section);
                if(pli) {
                    DPRINTF(E_DBG,L_CONF,"Found existing section\n");
                    pllcurrent = pli->value.as_ll;
                } else {
                    DPRINTF(E_DBG,L_CONF,"creating new section\n");
                    if((err = ll_create(&pllcurrent)) != LL_E_SUCCESS) {
                        ll_destroy(pllnew);
                        DPRINTF(E_LOG,L_CONF,"Error creating linked list: %d\n",err);
                        io_close(hconfig);
                        io_dispose(hconfig);
                        return CONF_E_UNKNOWN;
                    }
                    ll_add_ll(pllnew,replaced_section,pllcurrent);
                }
                pce = _conf_get_keyinfo(replaced_section, replaced_key);

                key_type = CONF_T_STRING;
                if(pce)
                    key_type = pce->type;

                switch(key_type) {
                case CONF_T_MULTIPATH:
                case CONF_T_MULTICOMMA:
                    /* first, see if we already have a tree... */
                    pli = ll_fetch_item(pllcurrent,replaced_key);
                    if(!pli) {
                        if((ll_create(&plltemp) != LL_E_SUCCESS)) {
                            DPRINTF(E_FATAL,L_CONF,"Could not create "
                                    "linked list.\n");
                        }
                        ll_add_ll(pllcurrent,replaced_key,plltemp);
                        ll_set_flags(plltemp,0); /* allow dups */
                    } else {
                        plltemp = pli->value.as_ll;
                    }

                    /* got list, break comma sep and add */
                    if(util_split(value,",",&valuearray) >= 0) {
                        index = 0;
                        while(valuearray[index]) {
                            ll_add_string(plltemp,replaced_key,valuearray[index]);
                            index++;
                        }
                        util_dispose_split(valuearray);
                    } else {
                        ll_add_string(plltemp,replaced_key,value);
                    }
                    break;
                case CONF_T_INT:
                case CONF_T_STRING:
                case CONF_T_EXISTPATH:
                default:
                    ll_add_string(pllcurrent,replaced_key,value);
                    break;
                }


                if(comment) {
                    /* this is an inline comment */
                    snprintf(keybuffer,sizeof(keybuffer),"in_%s_%s",
                             replaced_section,replaced_key);
                    ll_add_string(pllcomment,keybuffer,comment);
                    comment = NULL;
                }

                if(prev_comments[0] != '\0') {
                    /* we had some preceding comments */
                    snprintf(keybuffer,sizeof(keybuffer),"pre_%s_%s",
                             replaced_section, replaced_key);
                    ll_add_string(pllcomment,keybuffer,prev_comments);
                    prev_comments[0] = '\0';
                    current_comment_length=0;
                }
            } else {
                ws=1;
            }

            if(((term) && (strlen(term))) && (!value)) {
                DPRINTF(E_LOG,L_CONF,"Error in config file on line %d\n",line);
                DPRINTF(E_LOG,L_CONF,"key: %s, value: %s\n",replaced_key,value);
                ll_destroy(pllnew);
                return CONF_E_PARSE;
            }
        }

        if((comment)||(ws)) {
            if(!comment)
                comment = "";

            /* add to prev comments */
            while((current_comment_length + (int)strlen(comment) + 2 >=
                   total_comment_length) && (total_comment_length < 32768)) {
                total_comment_length *= 2;
                DPRINTF(E_DBG,L_CONF,"Expanding precomments to %d\n",
                        total_comment_length);
                prev_comments=realloc(prev_comments,total_comment_length);
                if(!prev_comments)
                    DPRINTF(E_FATAL,L_CONF,"Malloc error\n");
            }

            if(current_comment_length + (int)strlen(comment)+2 >=
               total_comment_length) {
                if(!warned_truncate)
                    DPRINTF(E_LOG,L_CONF,"Truncating comments in config\n");
                warned_truncate=1;
            } else {
                if(strlen(comment)) {
                    strcat(prev_comments,"#");
                    strcat(prev_comments,comment);
                    current_comment_length += ((int) strlen(comment) + 1);
                } else {
                    strcat(prev_comments,"\n");
                    current_comment_length += 2; /* windows, worst case */
                }
            }
        }
        len = sizeof(linebuffer);
    }

    if(section_name) {
        free(section_name);
        section_name = NULL;
    }

    if(prev_comments) {
        if(prev_comments[0] != '\0') {
            ll_add_string(pllcomment,"end",prev_comments);
        }
        free(prev_comments);
        prev_comments = NULL;
    }

    io_close(hconfig);
    io_dispose(hconfig);

    /*  Sanity check */
    if(_conf_verify(pllnew)) {
        DPRINTF(E_INF,L_CONF,"Loading new config file.\n");
        util_mutex_lock(l_conf);
        _conf_apply(pllnew);
        if(conf_main) {
            ll_destroy(conf_main);
        }

        if(conf_comments) {
            ll_destroy(conf_comments);
        }

        conf_main = pllnew;
        conf_comments = pllcomment;
        util_mutex_unlock(l_conf);
    } else {
        ll_destroy(pllnew);
        ll_destroy(pllcomment);
        DPRINTF(E_LOG,L_CONF,"Could not validate config file.  Ignoring\n");
        return CONF_E_BADCONFIG;
    }

    return CONF_E_SUCCESS;
}

/**
 * do final config file shutdown
 */
int conf_close(void) {
    if(conf_main) {
        ll_destroy(conf_main);
        conf_main = NULL;
    }

    if(conf_comments) {
        ll_destroy(conf_comments);
        conf_comments = NULL;
    }

    if(conf_main_file) {
        free(conf_main_file);
        conf_main_file = NULL;
    }

    return CONF_E_SUCCESS;
}


/**
 * read a value from the CURRENT config tree as an integer
 *
 * @param section section name to search in
 * @param key key to search for
 * @param dflt default value to return if key not found
 * @returns value as integer if found, dflt value otherwise
 */
int conf_get_int(char *section, char *key, int dflt) {
    LL_ITEM *pitem;
    int retval;

    util_mutex_lock(l_conf);
    pitem = _conf_fetch_item(conf_main,section,key);
    if((!pitem) || (pitem->type != LL_TYPE_STRING)) {
        retval = dflt;
    } else {
        retval = atoi(pitem->value.as_string);
    }
    util_mutex_unlock(l_conf);

    return retval;
}

/**
 * read a value from the CURRENT config tree as a string
 *
 * @param section section name to search in
 * @param key key to search for
 * @param dflt default value to return if key not found
 * @param out buffer to put resulting string in
 * @param size pointer to size of buffer
 * @returns CONF_E_SUCCESS with out filled on success,
 *          or CONF_E_OVERFLOW, with size set to required buffer size
 */
int conf_get_string(char *section, char *key, char *dflt, char *out, int *size) {
    LL_ITEM *pitem;
    char *result;
    int len;

    util_mutex_lock(l_conf);
    pitem = _conf_fetch_item(conf_main,section,key);
    if((!pitem) || (pitem->type != LL_TYPE_STRING)) {
        result = dflt;
    } else {
        result = pitem->value.as_string;
    }

    if(!result) {
        util_mutex_unlock(l_conf);
        return CONF_E_NOTFOUND;
    }

    len = (int) strlen(result) + 1;

    if(len <= *size) {
        *size = len;
        strcpy(out,result);
    } else {
        util_mutex_unlock(l_conf);
        *size = len;
        return CONF_E_OVERFLOW;
    }

    util_mutex_unlock(l_conf);
    return CONF_E_SUCCESS;
}

/**
 * return the value from the CURRENT config tree as an allocated string
 *
 * @param section section name to search in
 * @param key key to search for
 * @param dflt default value to return if key not found
 * @returns a pointer to an allocated string containing the required
 *          configuration key
 */
char *conf_alloc_string(char *section, char *key, char *dflt) {
    LL_ITEM *pitem;
    char *result;
    char *retval;

    util_mutex_lock(l_conf);
    pitem = _conf_fetch_item(conf_main,section,key);
    if((!pitem) || (pitem->type != LL_TYPE_STRING)) {
        result = dflt;
    } else {
        result = pitem->value.as_string;
    }

    if(result == NULL) {
        util_mutex_unlock(l_conf);
        return NULL;
    }

    retval = strdup(result);

    if(!retval) {
        DPRINTF(E_FATAL,L_CONF,"Malloc error in conf_alloc_string\n");
    }
    util_mutex_unlock(l_conf);
    return retval;
}


/**
 * set (update) the config tree with a particular value.
 * this accepts an int, but it actually adds it as a string.
 * in that sense, it's really just a wrapper for conf_set_string
 *
 * @param section section that the key is in
 * @param key key to update
 * @param value value to set it to
 * @returns E_CONF_SUCCESS on success, error code otherwise
 */
int conf_set_int(char *section, char *key, int value, int verify) {
    char buffer[40]; /* ?? */
    snprintf(buffer,sizeof(buffer),"%d",value);

    return conf_set_string(section, key, buffer, verify);
}

/**
 * set (update) the config tree with a particular string value
 *
 * @param section section that the key is in
 * @param key key to update
 * @param value value to set it to
 * @returns E_CONF_SUCCESS on success, error code otherwise
 */
int conf_set_string(char *section, char *key, char *value, int verify) {
    LL_ITEM *pitem;
    LL_ITEM *psection;
    LL *section_ll;
    LL *temp_ll;
    CONF_ELEMENTS *pce;
    int key_type = CONF_T_STRING;
    char **valuearray;
    int index;
    int err;
    char *oldvalue=NULL;
    LL_ITEM *polditem;

    util_mutex_lock(l_conf);

    /* verify the item */
    err=_conf_verify_element(section,key,value);
    if(err != CONF_E_SUCCESS) {
        util_mutex_unlock(l_conf);
        DPRINTF(E_DBG,L_CONF,"Couldn't validate %s = %s\n",key,value);
        return err;
    }

    if(verify) {
        util_mutex_unlock(l_conf);
        return CONF_E_SUCCESS;
    }

    pce = _conf_get_keyinfo(section,key);
    if(pce)
        key_type = pce->type;

    /* apply the config change, if necessary */
    if((key_type != CONF_T_MULTICOMMA) && (key_type != CONF_T_MULTIPATH)) {
        /* let's apply it */
        polditem = _conf_fetch_item(conf_main,section,key);
        if(polditem)
            oldvalue = polditem->value.as_string;
        _conf_apply_element(section,key,oldvalue,value);
    }

    if(strcmp(value,"") == 0) {
        /* deleting the item */
        pitem = ll_fetch_item(conf_main,section);
        if(!pitem) {
            util_mutex_unlock(l_conf);
            return CONF_E_SUCCESS;
        }

        section_ll = pitem->value.as_ll;
        if(!section_ll) {
            util_mutex_unlock(l_conf);
            return CONF_E_SUCCESS; /* ?? deleting an already deleted item */
        }

        /* don't care about item... might already be gone! */
        ll_del_item(section_ll,key);
        util_mutex_unlock(l_conf);
        return CONF_E_SUCCESS;
    }

    pitem = _conf_fetch_item(conf_main,section,key);
    if(!pitem) {
        /* fetch the section and add it to that list */
        if(!(psection = ll_fetch_item(conf_main,section))) {
            /* that subkey doesn't exist yet... */
            if((err = ll_create(&section_ll)) != LL_E_SUCCESS) {
                DPRINTF(E_LOG,L_CONF,"Could not create linked list: %d\n",err);
                util_mutex_unlock(l_conf);
                return CONF_E_UNKNOWN;
            }
            if((err=ll_add_ll(conf_main,section,section_ll)) != LL_E_SUCCESS) {
                DPRINTF(E_LOG,L_CONF,"Error inserting new subkey: %d\n",err);
                util_mutex_unlock(l_conf);
                return CONF_E_UNKNOWN;
            }
        } else {
            section_ll = psection->value.as_ll;
        }
        /* have the section, now add it */
        if((key_type == CONF_T_MULTICOMMA) || (key_type == CONF_T_MULTIPATH)) {
            if((err = ll_create(&temp_ll)) != LL_E_SUCCESS) {
                DPRINTF(E_FATAL,L_CONF,"conf_set_string: could not create ll\n");
            }
            ll_add_ll(section_ll,key,temp_ll);
            ll_set_flags(temp_ll,0); /* allow dups */
            if(util_split(value,",",&valuearray) >= 0) {
                index = 0;
                while(valuearray[index]) {
                    ll_add_string(temp_ll,key,valuearray[index]);
                    index++;
                }
                util_dispose_split(valuearray);
            }
        } else {
            if((err = ll_add_string(section_ll,key,value)) != LL_E_SUCCESS) {
                DPRINTF(E_LOG,L_CONF,"Error in conf_set_string: "
                        "(%s/%s)\n",section,key);
                util_mutex_unlock(l_conf);
                return CONF_E_UNKNOWN;
            }
        }
    } else {
        /* we have the item, let's update it */
        if((key_type == CONF_T_MULTICOMMA) || (key_type == CONF_T_MULTIPATH)) {
            /* delete whatever is there, then add from commas */
            ll_destroy(pitem->value.as_ll);
            if(ll_create(&pitem->value.as_ll) != LL_E_SUCCESS) {
                DPRINTF(E_FATAL,L_CONF,
                        "conf_set_string: could not create ll\n");
            }
            ll_set_flags(pitem->value.as_ll,0); /* allow dups */
            if(util_split(value,",",&valuearray) >= 0) {
                index = 0;
                while(valuearray[index]) {
                    ll_add_string(pitem->value.as_ll,key,valuearray[index]);
                    index++;
                }
                util_dispose_split(valuearray);
            }
        } else {
            ll_update_string(pitem,value);
        }
    }

    util_mutex_unlock(l_conf);
    return conf_write();
}

/**
 * determine if the configuration file is writable
 *
 * @returns TRUE if writable, FALSE otherwise
 */
int conf_iswritable(void) {
    int retval = FALSE;

    /* don't want configfile reopened under us */
    util_mutex_lock(l_conf);

    if(!conf_main_file)
        return FALSE;

    retval = !access(conf_main_file,W_OK);

    util_mutex_unlock(l_conf);
    return retval;
}

/**
 * write the current config tree back to the config file
 *
 */
int conf_write(void) {
    int retval = CONF_E_NOTWRITABLE;
    IOHANDLE outfile;

    if(!conf_main_file) {
        DPRINTF(E_DBG,L_CONF,"Config file apparently  not loaded\n");
        return CONF_E_NOCONF;
    }

    util_mutex_lock(l_conf);
    outfile = io_new();
    if(!outfile)
        DPRINTF(E_FATAL,L_CONF,"io_new failed in conf_write\n");

    if(io_open(outfile,"file://%U?mode=w&ascii=1",conf_main_file)) {
        retval = _conf_write(outfile,conf_main,0,NULL);
        io_close(outfile);
        io_dispose(outfile);
    } else {
        DPRINTF(E_LOG,L_CONF,"Error opening config file for write: %s\n",
            io_errstr(outfile));
        io_dispose(outfile);
    }
    util_mutex_unlock(l_conf);

    return retval;
}

/**
 * do the actual work of writing the config file
 *
 * @param fp file we are writing the config file to
 * @param pll list we are dumping k/v pairs for
 * @param sublevel whether this is the root, or a subkey
 * @returns CONF_E_SUCCESS on success, failure code otherwise
 */
int _conf_write(IOHANDLE hfile, LL *pll, int sublevel, char *parent) {
    LL_ITEM *pli;
    LL_ITEM *ppre, *pin;
    LL_ITEM *plitemp;
    int first;
    int err;

    char keybuffer[256];

    if(!pll)
        return CONF_E_SUCCESS; /* ?? */

    /* write all the solo keys, first! */
    pli = pll->itemlist.next;
    while(pli) {
        /* if there is a PRE there, then let's emit that*/
        if(sublevel) {
            snprintf(keybuffer,sizeof(keybuffer),"pre_%s_%s",parent,pli->key);
            ppre=ll_fetch_item(conf_comments,keybuffer);
            snprintf(keybuffer,sizeof(keybuffer),"in_%s_%s",parent,pli->key);
            pin = ll_fetch_item(conf_comments,keybuffer);
        } else {
            snprintf(keybuffer,sizeof(keybuffer),"pre_%s",pli->key);
            ppre=ll_fetch_item(conf_comments,keybuffer);
            snprintf(keybuffer,sizeof(keybuffer),"in_%s",pli->key);
            pin = ll_fetch_item(conf_comments,keybuffer);
        }

        if(ppre) {
            io_printf(hfile,"%s",ppre->value.as_string);
        }

        switch(pli->type) {
        case LL_TYPE_LL:
            if(sublevel) {
                /* must be multivalued */
                plitemp = NULL;
                first = 1;
                io_printf(hfile,"%s = ",pli->key);
                while((plitemp = ll_get_next(pli->value.as_ll,plitemp))) {
                    if(!first)
                        io_printf(hfile,",");
                    first=0;
                    io_printf(hfile,"%s",plitemp->value.as_string);
                }
                io_printf(hfile,"\n");
            } else {
                io_printf(hfile,"[%s]",pli->key);
                if(pin) {
                    io_printf(hfile," #%s",pin->value.as_string);
                }
                io_printf(hfile,"\n");

                if((err =_conf_write(hfile, pli->value.as_ll, 1, pli->key)) != CONF_E_SUCCESS) {
                    DPRINTF(E_DBG,L_CONF,"Error writing key %s:%s\n",pli->key);
                    return err;
                }
            }
            break;

        case LL_TYPE_INT:
            io_printf(hfile,"%s = %d",pli->key,pli->value.as_int);
            if(pin) {
                io_printf(hfile," #%s",pin->value.as_string);
            }
            io_printf(hfile,"\n");
            break;

        case LL_TYPE_STRING:
            io_printf(hfile,"%s = %s",pli->key,pli->value.as_string);
            if(pin) {
                io_printf(hfile," #%s",pin->value.as_string);
            }
            io_printf(hfile,"\n");
            break;
        }

        pli = pli->next;
    }

    if(!sublevel) {
        pin = ll_fetch_item(conf_comments,"end");
        if(pin) {
            io_printf(hfile,"%s",pin->value.as_string);
        }
    }

    return CONF_E_SUCCESS;
}


/**
 * determine if a configuration entry is actually set
 *
 * @param section section to test
 * @key key to check
 * @return TRUE if set, FALSE otherwise
 */
int conf_isset(char *section, char *key) {
    int retval = FALSE;

    util_mutex_lock(l_conf);
    if(_conf_fetch_item(conf_main,section,key)) {
        retval = TRUE;
    }
    util_mutex_unlock(l_conf);

    return retval;
}

/**
 * implode a multivalued term in a perl sense.
 *
 * @param section section of term to implode
 * @param key key of term to implode
 * @pararm delimiter what to "glue" them with
 * @returns imploded string (preallocated), or NULL
 */
char *conf_implode(char *section, char *key, char *delimiter) {
    LL_ITEM *pitem;
    LL_ITEM *penum;
    int count;
    int len;
    char *retval;

    util_mutex_lock(l_conf);
    pitem = _conf_fetch_item(conf_main,section,key);
    if((!pitem) || (pitem->type != LL_TYPE_LL)) {
        util_mutex_unlock(l_conf);
        return NULL;
    }

    /* otherwise, alloc a string and go */
    count = len = 0;
    penum = NULL;
    while((penum = ll_get_next(pitem->value.as_ll,penum))) {
        if(penum->type != LL_TYPE_STRING) {
            DPRINTF(E_FATAL,L_CONF,"multivalued property not a string?\n");
        }
        len += (int)strlen(penum->value.as_string);
        count++;
    }

    if(!count) {
        util_mutex_unlock(l_conf);
        return NULL;
    }

    len += ((int)strlen(delimiter) * (count-1));
    retval = (char*)malloc(len + 1);
    if(!retval) {
        DPRINTF(E_FATAL,L_CONF,"conf_implode: malloc\n");
    }

    memset(retval,0,len+1);
    penum = NULL;
    while((penum = ll_get_next(pitem->value.as_ll,penum))) {
        strcat(retval,penum->value.as_string);
        if(--count) {
            strcat(retval,delimiter);
        }
    }

    util_mutex_unlock(l_conf);
    return retval;
}

/**
 * return a multi-valued item as an array (values)
 *
 * @param section section to fetch
 * @param key multivalued key to get from array
 * @returns TRUE on success, FALSE on failure
 */
int conf_get_array(char *section, char *key, char ***argvp) {
    LL_ITEM *pitem, *penum;
    int count;
    int len;

    util_mutex_lock(l_conf);
    pitem = _conf_fetch_item(conf_main,section,key);
    if((!pitem) || (pitem->type != LL_TYPE_LL)) {
        util_mutex_unlock(l_conf);
        return FALSE;
    }

    /* otherwise, alloc a string and go */
    count = 0;
    penum = NULL;
    while((penum = ll_get_next(pitem->value.as_ll,penum))) {
        if(penum->type != LL_TYPE_STRING) {
            DPRINTF(E_FATAL,L_CONF,"multivalued property not a string?\n");
        }
        count++;
    }

    /* now we have a count, alloc an argv */
    len = (count+1) * sizeof(char*);
    *(argvp) = (char**)malloc(len);
    if(!*(argvp)) {
        DPRINTF(E_FATAL,L_CONF,"conf_get_array: malloc\n");
    }

    memset(*(argvp),0,len);

    count=0;
    penum=NULL;
    while((penum = ll_get_next(pitem->value.as_ll,penum))) {
        (*argvp)[count] = strdup(penum->value.as_string);
        if(!(*argvp)[count]) {
            DPRINTF(E_FATAL,L_CONF,"conf_get_array: malloc\n");
        }
        count++;
    }

    util_mutex_unlock(l_conf);
    return TRUE;
}

/**
 * dispose of the array created above
 *
 * @param argv argv pointer created
 */
void conf_dispose_array(char **argv) {
    int index=0;

    if(!argv)
        return;

    while(argv[index]) {
        free(argv[index]);
        index++;
    }
}

/* FIXME: this belongs in xml-rpc, but need config enumerating fns */

/**
 * dump the config to xml
 *
 * @param pwsc web connection to dump to
 * @returns TRUE on success, FALSE otherwise
 */
int conf_xml_dump(WS_CONNINFO *pwsc) {
    XMLSTRUCT *pxml;
    int retval;

    if(!conf_main_file) {
        return FALSE; /* CONF_E_NOCONF */
    }

    pxml = xml_init(pwsc,1);
    xml_push(pxml,"config");

    util_mutex_lock(l_conf);

    retval = _conf_xml_dump(pxml,conf_main,0,NULL);

    util_mutex_unlock(l_conf);

    xml_pop(pxml);
    xml_deinit(pxml);

    return retval;
}

/**
 * do the actual work of dumping the config file
 *
 * @param pwsc web connection we are writing the config file to
 * @param pll list we are dumping k/v pairs for
 * @param sublevel whether this is the root, or a subkey
 * @returns TRUE on success, FALSE otherwise
 */
int _conf_xml_dump(XMLSTRUCT *pxml, LL *pll, int sublevel, char *parent) {
    LL_ITEM *pli;
    LL_ITEM *plitemp;

    if(!pll)
        return TRUE;

    /* write all the solo keys, first! */
    pli = pll->itemlist.next;
    while(pli) {
        switch(pli->type) {
        case LL_TYPE_LL:
            if(sublevel) {
                /* must be multivalued */
                plitemp = NULL;
                xml_push(pxml,pli->key);
                while((plitemp = ll_get_next(pli->value.as_ll,plitemp))) {
                    xml_output(pxml,"item","%s",plitemp->value.as_string);
                }
                xml_pop(pxml);
            } else {
                xml_push(pxml,pli->key);
                if(!_conf_xml_dump(pxml, pli->value.as_ll, 1, pli->key))
                   return FALSE;
                xml_pop(pxml);
            }
            break;

        case LL_TYPE_INT:
            xml_output(pxml,pli->key,"%d",pli->value.as_int);
            break;

        case LL_TYPE_STRING:
            xml_output(pxml,pli->key,"%s",pli->value.as_string);
            break;
        }

        pli = pli->next;
    }

    return TRUE;
}

/**
 * get the filename of the currently runnig config file
 *
 * @returns path if it exists, or NULL if no config file opened
 */
char *conf_get_filename(void) {
    return conf_main_file;
}


/**
 * this is an ugly block of crap to carry around every
 * time one wants the servername.
 */
char *conf_get_servername(void) {
    char *retval;
    char *default_name = "firefly %v on %h";
    char hostname[HOST_NAME_MAX + 1]; /* " Firefly Server" */
    char *template, *src, *dst;
    int len;

    gethostname(hostname,HOST_NAME_MAX);
    retval = strchr(hostname,'.');
    if(retval) *retval = '\0';

    retval = hostname;
    while(*retval) {
        *retval = tolower(*retval);
        retval++;
    }

    template = conf_alloc_string("general","servername",default_name);
    src = template;
    len = 0;
    while(*src) {
        if(*src == '%') {
            src++;
            switch(*src) {
            case 'h':
                len += strlen(hostname);
                src++;
                break;
            case 'v':
                len += strlen(VERSION);
                src++;
                break;
            default:
                len += 2;
                src++;
                break;
            }
        } else {
            len++;
            src++;
        }
    }

    retval = (char*)malloc(len+1);
    if(!retval) DPRINTF(E_FATAL,L_MISC,"malloc");

    memset(retval,0,len+1);

    dst = retval;
    src = template;

    while(*src) {
        if(*src == '%') {
            src++;
            switch(*src) {
            case 'h':
                strcat(dst,hostname);
                dst += strlen(hostname);
                src ++;
                break;
            case 'v':
                strcat(dst,VERSION);
                dst += strlen(VERSION);
                src++;
                break;
            default:
                *dst++ = '%';
                *dst++ = *src++;
                break;
            }
        } else {
            *dst++ = *src++;
        }

    }

    if(strlen(retval) > MAX_REND_LEN) {
        retval[MAX_REND_LEN] = '\0';
        retval[MAX_REND_LEN-1] = '.';
        retval[MAX_REND_LEN-2] = '.';
        retval[MAX_REND_LEN-3] = '.';
    }

    free(template);
    return retval;
}
